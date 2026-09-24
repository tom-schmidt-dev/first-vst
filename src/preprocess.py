#!/usr/bin/env python3
import glob
import os
import h5py
import librosa
import numpy as np
import soundfile as sf

TARGET_SR = 44100
CHUNK_DURATION = 4.0
CHUNK_SAMPLES = int(TARGET_SR * CHUNK_DURATION)
HOP_LENGTH = 128
FRAME_COUNT = CHUNK_SAMPLES // HOP_LENGTH

F0_MIN = 150.0   # Untere Grenze Violine (G3 ca. 196 Hz)
F0_MAX = 2500.0  # Obere Grenze
F0_NORM_MAX = 3000.0
LOUDNESS_FLOOR_DB = -80.0


def compute_loudness(audio_chunk: np.ndarray, hop_length: int) -> np.ndarray:
    frame_length = hop_length * 2
    rms = librosa.feature.rms(
        y=audio_chunk,
        frame_length=frame_length,
        hop_length=hop_length,
        center=True
    )[0]

    if len(rms) > FRAME_COUNT:
        rms = rms[:FRAME_COUNT]
    elif len(rms) < FRAME_COUNT:
        rms = np.pad(rms, (0, FRAME_COUNT - len(rms)), mode="edge")

    db = 20.0 * np.log10(np.maximum(rms, 1e-5))
    db_clipped = np.clip(db, LOUDNESS_FLOOR_DB, 0.0)
    normalized_loudness = (db_clipped - LOUDNESS_FLOOR_DB) / (-LOUDNESS_FLOOR_DB)
    return normalized_loudness.astype(np.float32)


def compute_f0(audio_chunk: np.ndarray, sr: int, hop_length: int) -> np.ndarray:
    f0, _, _ = librosa.pyin(
        audio_chunk,
        fmin=F0_MIN,
        fmax=F0_MAX,
        sr=sr,
        hop_length=hop_length,
        frame_length=hop_length * 4,
        fill_na=0.0
    )

    if len(f0) > FRAME_COUNT:
        f0 = f0[:FRAME_COUNT]
    elif len(f0) < FRAME_COUNT:
        f0 = np.pad(f0, (0, FRAME_COUNT - len(f0)), mode="constant", constant_values=0.0)

    f0 = np.nan_to_num(f0, nan=0.0, posinf=0.0, neginf=0.0)
    f0_normalized = np.clip(f0 / F0_NORM_MAX, 0.0, 1.0)
    return f0_normalized.astype(np.float32)


def process_audio_file(file_path: str):
    audio, sr = sf.read(file_path)

    if audio.ndim > 1:
        audio = np.mean(audio, axis=1)

    if sr != TARGET_SR:
        audio = librosa.resample(audio, orig_sr=sr, target_sr=TARGET_SR)

    peak = np.max(np.abs(audio))
    if peak > 0.0:
        audio = audio / peak

    total_samples = len(audio)
    chunks = []

    # Falls Datei kürzer als CHUNK_SAMPLES ist, mit Nullen auffüllen
    if total_samples < CHUNK_SAMPLES:
        padded = np.pad(audio, (0, CHUNK_SAMPLES - total_samples), mode="constant")
        chunks.append(padded.astype(np.float32))
    else:
        # 50% Overlap
        step = CHUNK_SAMPLES // 2
        for start in range(0, total_samples - CHUNK_SAMPLES + 1, step):
            chunk = audio[start:start + CHUNK_SAMPLES]
            chunks.append(chunk.astype(np.float32))

    return chunks


def main():
    input_dir = "data/raw/violin"
    output_h5 = "data/dataset.h5"

    audio_files = sorted(glob.glob(os.path.join(input_dir, "**", "*.wav"), recursive=True))

    if not audio_files:
        raise FileNotFoundError(f"Keine WAV-Dateien in {input_dir} gefunden.")

    all_audio = []
    all_f0 = []
    all_loudness = []

    print(f"Gefundene Audiodateien: {len(audio_files)}. Starte Extraktion...")

    for idx, fpath in enumerate(audio_files):
        chunks = process_audio_file(fpath)
        for chunk in chunks:
            f0_norm = compute_f0(chunk, TARGET_SR, HOP_LENGTH)
            loudness_norm = compute_loudness(chunk, HOP_LENGTH)

            all_audio.append(chunk)
            all_f0.append(f0_norm)
            all_loudness.append(loudness_norm)

        if (idx + 1) % 25 == 0 or idx + 1 == len(audio_files):
            print(f"Fortschritt: {idx + 1}/{len(audio_files)} Dateien | Chunks gesamt: {len(all_audio)}")

    audio_arr = np.array(all_audio, dtype=np.float32)
    f0_arr = np.array(all_f0, dtype=np.float32)
    loudness_arr = np.array(all_loudness, dtype=np.float32)

    # Validierungs-Assertions
    assert not np.isnan(audio_arr).any(), "Audio enthält NaN"
    assert not np.isnan(f0_arr).any(), "f0 enthält NaN"
    assert not np.isnan(loudness_arr).any(), "Loudness enthält NaN"
    assert 0.0 <= f0_arr.min() and f0_arr.max() <= 1.0, "f0 Wertebereich verletzt"
    assert 0.0 <= loudness_arr.min() and loudness_arr.max() <= 1.0, "Loudness Wertebereich verletzt"

    with h5py.File(output_h5, "w") as h5f:
        h5f.create_dataset("audio", data=audio_arr, compression="gzip")
        h5f.create_dataset("f0", data=f0_arr, compression="gzip")
        h5f.create_dataset("loudness", data=loudness_arr, compression="gzip")

    print(f"Datensatz erfolgreich erstellt: {output_h5}")
    print(f"Shapes -> Audio: {audio_arr.shape}, f0: {f0_arr.shape}, Loudness: {loudness_arr.shape}")


if __name__ == "__main__":
    main()
