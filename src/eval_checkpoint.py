#!/usr/bin/env python3
import h5py
import torch
import soundfile as sf
import os
from model import DDSPModel

def main():
    device = torch.device("cuda:0" if torch.cuda.is_available() else "cpu")
    ckpt_path = "checkpoints/ddsp_epoch_40.pt"
    output_wav = "data/test_reconstruction.wav"
    target_wav = "data/test_target.wav"

    if not os.path.exists(ckpt_path):
        raise FileNotFoundError(f"Checkpoint nicht gefunden: {ckpt_path}")

    # Modell initialisieren und Gewichte laden
    model = DDSPModel().to(device)
    model.load_state_dict(torch.load(ckpt_path, map_location=device))
    model.eval()

    # Einen Chunk aus dem Datensatz laden
    with h5py.File("data/dataset.h5", "r") as f:
        audio_ref = f["audio"][0]
        f0 = torch.from_numpy(f["f0"][0]).unsqueeze(0).unsqueeze(-1).to(device)
        loudness = torch.from_numpy(f["loudness"][0]).unsqueeze(0).unsqueeze(-1).to(device)

    target_samples = len(audio_ref)

    # Inferenz
    with torch.no_grad():
        pred_audio, harmonic_audio, noise_audio = model(f0, loudness, target_samples)

    pred_np = pred_audio.squeeze(0).cpu().numpy()

    # Normalisierung zur Vermeidung von Clipping
    peak = max(abs(pred_np.max()), abs(pred_np.min()))
    if peak > 1.0:
        pred_np = pred_np / peak

    sf.write(output_wav, pred_np, 44100)
    sf.write(target_wav, audio_ref, 44100)

    print(f"Synthese erfolgreich:")
    print(f"- Rekonstruktion: {output_wav}")
    print(f"- Original:       {target_wav}")
    print(f"- Min/Max Werte:  {pred_np.min():.3f} / {pred_np.max():.3f}")

if __name__ == "__main__":
    main()
