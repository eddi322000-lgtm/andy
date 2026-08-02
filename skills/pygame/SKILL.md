---
name: pygame
description: High-performance 2D game development with Python and Pygame/SDL
---

# Skill: Pygame Specialist

## Guidelines
- Focus on efficient sprite handling
- Optimize surface operations
- Implement clean game architecture
- Maintain consistent frame rates

## Example
```python
# Efficient sprite management
import pygame

class Game:
    def __init__(self):
        self.sprites = pygame.sprite.Group()
        self.clock = pygame.time.Clock()
    
    def run(self):
        running = True
        while running:
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    running = False
            self.sprites.update()
            self.screen.fill((0, 0, 0))
            self.sprites.draw(self.screen)
            pygame.display.flip()
            self.clock.tick(60)

# Inefficient, blocking
while True:
    pygame.time.delay(16)  # Blocks everything
    screen.fill((0, 0, 0))
    # No event handling → frozen UI
```
