#!/usr/bin/env python3
import torch
import torch.nn as nn
from typing import List


class MultiScaleSpectralLoss(nn.Module):
    """Berechnet die Distanz zwischen linearen und logarithmischen Spektrogrammen über mehrere FFT-Größen."""
    def __init__(self, fft_sizes: List[int] = [2048, 1024, 512, 256, 128, 64]):
        super().__init__()
        self.fft_sizes = fft_sizes

    def forward(self, target: torch.Tensor, pred: torch.Tensor) -> torch.Tensor:
        total_loss = 0.0
        
        for n_fft in self.fft_sizes:
            hop_length = n_fft // 4
            window = torch.hann_window(n_fft, device=target.device)
            
            target_stft = torch.stft(
                target,
                n_fft=n_fft,
                hop_length=hop_length,
                window=window,
                return_complex=True,
                center=True
            )
            pred_stft = torch.stft(
                pred,
                n_fft=n_fft,
                hop_length=hop_length,
                window=window,
                return_complex=True,
                center=True
            )
            
            target_mag = torch.abs(target_stft)
            pred_mag = torch.abs(pred_stft)
            
            # Linearer L1 Loss
            linear_loss = torch.mean(torch.abs(target_mag - pred_mag))
            
            # Logarithmischer L1 Loss
            log_target = torch.log(torch.clamp(target_mag, min=1e-5))
            log_pred = torch.log(torch.clamp(pred_mag, min=1e-5))
            log_loss = torch.mean(torch.abs(log_target - log_pred))
            
            total_loss += (linear_loss + log_loss)
            
        return total_loss / len(self.fft_sizes)
