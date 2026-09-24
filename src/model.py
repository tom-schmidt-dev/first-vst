#!/usr/bin/env python3
import torch
import torch.nn as nn
import torch.nn.functional as F
import numpy as np


class HarmonicOscillator(nn.Module):
    """Additiver Synthesizer mit Phasenakkumulation und Anti-Aliasing."""
    def __init__(self, sample_rate: int = 44100, num_harmonics: int = 60):
        super().__init__()
        self.sample_rate = sample_rate
        self.num_harmonics = num_harmonics
        # Harmonische Multiplikatoren: [1, 2, ..., num_harmonics]
        self.register_buffer("harmonic_multipliers", torch.arange(1, num_harmonics + 1, dtype=torch.float32))

    def forward(self, f0_upsampled: torch.Tensor, harmonic_amps_upsampled: torch.Tensor) -> torch.Tensor:
        """
        f0_upsampled: (Batch, Samples, 1) in Hz
        harmonic_amps_upsampled: (Batch, Samples, num_harmonics)
        """
        # Phaseninkrement pro Sample berechnen
        # Delta phi = 2 * pi * f0 / fs
        phase_increments = (2.0 * np.pi * f0_upsampled) / self.sample_rate
        
        # Phasenakkumulation
        base_phase = torch.cumsum(phase_increments, dim=1)
        
        # Harmonische Phasen: (Batch, Samples, num_harmonics)
        harmonic_phases = base_phase * self.harmonic_multipliers[None, None, :]
        
        # Anti-Aliasing Maske: Obertöne oberhalb Nyquist (fs / 2) abschneiden
        harmonic_frequencies = f0_upsampled * self.harmonic_multipliers[None, None, :]
        nyquist = self.sample_rate / 2.0
        anti_alias_mask = (harmonic_frequencies < nyquist).float()
        
        # Amplituden mit Maske filtern
        masked_amplitudes = harmonic_amps_upsampled * anti_alias_mask
        
        # Sinusschwingungen summieren
        harmonics = masked_amplitudes * torch.sin(harmonic_phases)
        audio = torch.sum(harmonics, dim=-1)
        return audio


class FilteredNoiseSynthesizer(nn.Module):
    """Synthese von gefiltertem Rauschen über spektrale Koeffizienten."""
    def __init__(self, num_filter_bands: int = 65, hop_length: int = 128):
        super().__init__()
        self.num_filter_bands = num_filter_bands
        self.hop_length = hop_length
        self.win_length = hop_length * 2  # 256 Samples für 50% Überlappung
        self.n_fft = self.win_length

    def forward(self, noise_magnitudes: torch.Tensor, target_samples: int) -> torch.Tensor:
        """
        noise_magnitudes: (Batch, Frames, num_filter_bands)
        """
        batch_size = noise_magnitudes.shape[0]

        # Weißes Rauschen im Zeitbereich generieren
        noise = torch.randn(batch_size, target_samples, device=noise_magnitudes.device)

        # STFT des Rauschens berechnen
        window = torch.hann_window(self.win_length, device=noise.device)
        noise_stft = torch.stft(
            noise,
            n_fft=self.n_fft,
            hop_length=self.hop_length,
            win_length=self.win_length,
            window=window,
            return_complex=True,
            center=True
        )  # Shape: (Batch, 129, STFT_Frames)

        # Dimensionen für Interpolation vorbereiten: (Batch, 1, num_filter_bands, Frames)
        mags = noise_magnitudes.permute(0, 2, 1).unsqueeze(1)

        # Bilineare Interpolation auf 129 Bins und STFT-Framelänge
        target_freq_bins = self.n_fft // 2 + 1
        target_frames = noise_stft.shape[-1]
        mags = F.interpolate(
            mags,
            size=(target_freq_bins, target_frames),
            mode="bilinear",
            align_corners=False
        ).squeeze(1)  # Shape: (Batch, 129, STFT_Frames)

        # Spektrale Filterung
        filtered_stft = noise_stft * mags

        # Inverse STFT (COLA-Bedingung erfüllt)
        filtered_noise = torch.istft(
            filtered_stft,
            n_fft=self.n_fft,
            hop_length=self.hop_length,
            win_length=self.win_length,
            window=window,
            length=target_samples,
            center=True
        )
        return filtered_noise

class DDSPDecoder(nn.Module):
    """Mappt Steuerparameter (f0, Loudness) auf Synthesizer-Parameter."""
    def __init__(self, num_harmonics: int = 60, num_filter_bands: int = 65, hidden_dim: int = 128):
        super().__init__()
        self.num_harmonics = num_harmonics
        self.num_filter_bands = num_filter_bands
        
        self.input_dense = nn.Sequential(
            nn.Linear(2, hidden_dim),
            nn.LayerNorm(hidden_dim),
            nn.ReLU()
        )
        self.gru = nn.GRU(hidden_dim, hidden_dim, batch_first=True)
        
        # Ausgänge
        self.harmonic_dist_out = nn.Linear(hidden_dim, num_harmonics)
        self.harmonic_amp_out = nn.Linear(hidden_dim, 1)
        self.noise_out = nn.Linear(hidden_dim, num_filter_bands)

    def forward(self, f0: torch.Tensor, loudness: torch.Tensor):
        """
        f0: (Batch, Frames, 1)
        loudness: (Batch, Frames, 1)
        """
        x = torch.cat([f0, loudness], dim=-1)
        x = self.input_dense(x)
        gru_out, _ = self.gru(x)
        
        # Harmonische Amplituden (relative Verteilung via Softmax * Gesamtlautstärke via Sigmoid)
        harmonic_dist = F.softmax(self.harmonic_dist_out(gru_out), dim=-1)
        overall_amp = torch.sigmoid(self.harmonic_amp_out(gru_out))
        harmonic_amps = harmonic_dist * overall_amp
        
        # Rauschfilter-Amplituden (modifizierte Sigmoid-Aktivierung)
        noise_mags = torch.sigmoid(self.noise_out(gru_out))
        
        return harmonic_amps, overall_amp, noise_mags


class DDSPModel(nn.Module):
    """Gesamtes DDSP-Modell."""
    def __init__(
        self,
        sample_rate: int = 44100,
        hop_length: int = 128,
        num_harmonics: int = 60,
        num_filter_bands: int = 65,
        hidden_dim: int = 128,
        f0_scale_hz: float = 3000.0
    ):
        super().__init__()
        self.sample_rate = sample_rate
        self.hop_length = hop_length
        self.f0_scale_hz = f0_scale_hz
        
        self.decoder = DDSPDecoder(num_harmonics, num_filter_bands, hidden_dim)
        self.harmonic_oscillator = HarmonicOscillator(sample_rate, num_harmonics)
        self.noise_synth = FilteredNoiseSynthesizer(num_filter_bands, hop_length)

    def forward(self, f0: torch.Tensor, loudness: torch.Tensor, target_samples: int):
        # Inferenz Decoder
        harmonic_amps, overall_amp, noise_mags = self.decoder(f0, loudness)
        
        # Denormalisierung von f0 in Hertz für den Oszillator
        f0_hz = f0 * self.f0_scale_hz

        # Zeitliche Interpolation (Upsampling von Frame-Rate auf Audio-Sample-Rate)
        f0_upsampled = F.interpolate(
            f0_hz.transpose(1, 2),
            size=target_samples,
            mode="linear",
            align_corners=False
        ).transpose(1, 2)
        
        harmonic_amps_upsampled = F.interpolate(
            harmonic_amps.transpose(1, 2),
            size=target_samples,
            mode="linear",
            align_corners=False
        ).transpose(1, 2)

        # Synthese
        harmonic_audio = self.harmonic_oscillator(f0_upsampled, harmonic_amps_upsampled)
        noise_audio = self.noise_synth(noise_mags, target_samples)
        
        total_audio = harmonic_audio + noise_audio
        return total_audio, harmonic_audio, noise_audio
