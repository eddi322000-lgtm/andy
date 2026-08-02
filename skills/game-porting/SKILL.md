---
name: game-porting
description: Game engine translation and cross-platform game porting between Pygame, Godot, Unity, and HTML5
---

# Skill: Game Porting

## Guidelines
- Analyze source engine concepts (logic, rendering, physics)
- Map accurately to target engine
- Ensure game feel remains consistent across platforms
- Document engine-specific adaptations

## Example
```python
# Pygame → HTML5 (Phaser) Mapping
# Pygame:
#   clock.tick(60)
#   keys = pygame.key.get_pressed()
#   if keys[pygame.K_SPACE]: jump()

# Phaser:
#   this.time.addEvent({ delay: 1000/60, callback: update });
#   this.input.keyboard.on('keydown-SPACE', () => jump());
```
