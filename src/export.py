#!/usr/bin/env python3
import os
import torch
from model import DDSPModel, DDSPDecoder

def main():
    device = torch.device("cpu")
    ckpt_path = "checkpoints/ddsp_epoch_40.pt"
    export_dir = "exported_models"
    os.makedirs(export_dir, exist_ok=True)

    if not os.path.exists(ckpt_path):
        raise FileNotFoundError(f"Checkpoint nicht gefunden: {ckpt_path}")

    # Gesamtmodell initialisieren und Gewichte laden
    full_model = DDSPModel()
    full_model.load_state_dict(torch.load(ckpt_path, map_location=device))
    full_model.eval()

    # Nur den Decoder für die Plugin-Inferenz extrahieren
    decoder = full_model.decoder
    decoder.eval()

    # Dummy-Inputs für 1 Frame (Streaming-Modus)
    # Shape: (Batch=1, Frames=1, FeatureDim=1)
    dummy_f0 = torch.tensor([[[0.25]]], dtype=torch.float32)
    dummy_loudness = torch.tensor([[[0.80]]], dtype=torch.float32)

    # 1. TorchScript Export
    ts_path = os.path.join(export_dir, "ddsp_decoder.pt")
    traced_decoder = torch.jit.trace(decoder, (dummy_f0, dummy_loudness))
    traced_decoder.save(ts_path)
    print(f"TorchScript-Modell gespeichert: {ts_path}")

    # 2. ONNX Export
    onnx_path = os.path.join(export_dir, "ddsp_decoder.onnx")
    torch.onnx.export(
        decoder,
        (dummy_f0, dummy_loudness),
        onnx_path,
        export_params=True,
        opset_version=14,
        do_constant_folding=True,
        input_names=["f0", "loudness"],
        output_names=["harmonic_amps", "overall_amp", "noise_mags"],
        dynamic_axes={
            "f0": {1: "frames"},
            "loudness": {1: "frames"},
            "harmonic_amps": {1: "frames"},
            "overall_amp": {1: "frames"},
            "noise_mags": {1: "frames"}
        }
    )
    print(f"ONNX-Modell gespeichert: {onnx_path}")

    # Numerische Konsistenzprüfung
    with torch.no_grad():
        ref_h_amps, ref_amp, ref_noise = decoder(dummy_f0, dummy_loudness)

    # Test TorchScript
    ts_loaded = torch.jit.load(ts_path)
    ts_h_amps, ts_amp, ts_noise = ts_loaded(dummy_f0, dummy_loudness)
    
    assert torch.allclose(ref_h_amps, ts_h_amps, atol=1e-5), "TorchScript Output weicht ab"
    assert torch.allclose(ref_amp, ts_amp, atol=1e-5), "TorchScript Output weicht ab"
    assert torch.allclose(ref_noise, ts_noise, atol=1e-5), "TorchScript Output weicht ab"
    print("Numerische Konsistenzprüfung TorchScript: Erfolgreich.")

if __name__ == "__main__":
    main()
