#!/usr/bin/env python3
import os
import h5py
import torch
from torch.utils.data import Dataset, DataLoader
from model import DDSPModel
from loss import MultiScaleSpectralLoss


class ViolinDataset(Dataset):
    def __init__(self, h5_path: str):
        super().__init__()
        self.h5_path = h5_path
        with h5py.File(h5_path, "r") as f:
            self.length = f["audio"].shape[0]

    def __len__(self):
        return self.length

    def __getitem__(self, idx):
        # Datei pro Zugriff öffnen, um Multiprocessing-Locks zu verhindern
        with h5py.File(self.h5_path, "r") as f:
            audio = torch.from_numpy(f["audio"][idx])
            f0 = torch.from_numpy(f["f0"][idx]).unsqueeze(-1)          # (Frames, 1)
            loudness = torch.from_numpy(f["loudness"][idx]).unsqueeze(-1)  # (Frames, 1)
        return audio, f0, loudness


def main():
    device = torch.device("cuda:0" if torch.cuda.is_available() else "cpu")
    print(f"Verwende Recheneinheit: {device}")
    
    h5_path = "data/dataset.h5"
    checkpoint_dir = "checkpoints"
    os.makedirs(checkpoint_dir, exist_ok=True)
    
    dataset = ViolinDataset(h5_path)
    train_size = int(0.9 * len(dataset))
    val_size = len(dataset) - train_size
    train_set, val_set = torch.utils.data.random_split(dataset, [train_size, val_size])
    
    train_loader = DataLoader(train_set, batch_size=16, shuffle=True, drop_last=True)
    val_loader = DataLoader(val_set, batch_size=16, shuffle=False)
    
    model = DDSPModel().to(device)
    criterion = MultiScaleSpectralLoss().to(device)
    optimizer = torch.optim.Adam(model.parameters(), lr=1e-3)
    scheduler = torch.optim.lr_scheduler.ReduceLROnPlateau(optimizer, mode="min", factor=0.5, patience=5)

    epochs = 40
    print(f"Starte Training ({epochs} Epochen, {len(train_loader)} Batches pro Epoche)...")

    for epoch in range(1, epochs + 1):
        model.train()
        train_loss = 0.0

        for audio, f0, loudness in train_loader:
            audio = audio.to(device)
            f0 = f0.to(device)
            loudness = loudness.to(device)
            target_samples = audio.shape[1]

            optimizer.zero_grad()
            pred_audio, _, _ = model(f0, loudness, target_samples)
            loss = criterion(audio, pred_audio)
            loss.backward()
            optimizer.step()

            train_loss += loss.item()

        avg_train_loss = train_loss / len(train_loader)

        # Validierung
        model.eval()
        val_loss = 0.0
        with torch.no_grad():
            for audio, f0, loudness in val_loader:
                audio = audio.to(device)
                f0 = f0.to(device)
                loudness = loudness.to(device)
                target_samples = audio.shape[1]

                pred_audio, _, _ = model(f0, loudness, target_samples)
                v_loss = criterion(audio, pred_audio)
                val_loss += v_loss.item()

        avg_val_loss = val_loss / len(val_loader)
        scheduler.step(avg_val_loss)

        print(f"Epoche {epoch:02d}/{epochs:02d} | Train Loss: {avg_train_loss:.4f} | Val Loss: {avg_val_loss:.4f}")

        # Checkpoint speichern
        if epoch % 10 == 0 or epoch == epochs:
            ckpt_path = os.path.join(checkpoint_dir, f"ddsp_epoch_{epoch:02d}.pt")
            torch.save(model.state_dict(), ckpt_path)
            print(f"Modell gespeichert: {ckpt_path}")


if __name__ == "__main__":
    main()
