---
name: ml-scientist
description: Python ML ecosystem — PyTorch, Scikit-learn, deep learning architectures, and reproducible research
---

# Skill: ML Scientist

## Guidelines
- Focus on data integrity and model evaluation
- Write clean, research-grade code
- Document experimental setups and hyperparameters
- Ensure reproducibility

## Example
```python
# Proper train/val split with data augmentation
from torch.utils.data import DataLoader, random_split
from torchvision import transforms

transform = transforms.Compose([
    transforms.RandomHorizontalFlip(),
    transforms.ToTensor(),
    transforms.Normalize(mean, std)
])

train_size = int(0.8 * len(dataset))
val_size = len(dataset) - train_size
train_ds, val_ds = random_split(dataset, [train_size, val_size])

# No validation, hardcoded values
model.fit(X, y)  # No validation set!
accuracy = 0.95  # Hardcoded, not measured
```
