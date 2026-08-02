---
name: ui-designer
description: Framework-agnostic UI design for web and desktop — accessibility, responsive design, Tailwind, CSS, PyQt/Tkinter
---

# Skill: UI Designer

## Guidelines

- Build performant, accessible, and beautiful interfaces across web and desktop
- Prefer utility-first CSS (Tailwind) and CSS custom properties for theming
- Design ergonomics first: intuitive navigation, clear focus indicators, readable contrast
- Ensure responsive design across all screen sizes and input methods
- Keep implementations framework-agnostic — principles apply to React, Vue, PyQt, Tkinter, HTML5 Canvas

## Design Principles

1. **Accessibility (WCAG)**: Semantic HTML, ARIA labels, keyboard navigation, focus management, screen reader compatibility
2. **Responsive Typography**: Fluid scaling, modern design tokens, CSS custom properties for theming
3. **Tailwind Hygiene**: Utility-first classes, avoid redundant custom CSS, prefer design tokens over hardcoded values
4. **Framework Agnostic**: Comfortable with React, Vue, PyQt, Tkinter, HTML5 Canvas — adapt patterns to the target framework
5. **Animations & Transitions**: GPU-accelerated CSS transforms, smooth state changes, respect `prefers-reduced-motion`

## Verification Protocol

1. **ACCESSIBILITY CHECK**: Semantic HTML? ARIA labels? Keyboard navigation? Focus management? Screen reader compatible?
2. **RESPONSIVE CHECK**: Fluid layouts? Works on mobile, tablet, desktop? Responsive typography?
3. **PERFORMANCE CHECK**: GPU-accelerated animations? Lazy loading? No blocking calls in render/update loops?
4. **MAINTAINABILITY CHECK**: Tailwind utility-first? CSS custom properties for theming? No hardcoded values? Framework-agnostic patterns?

## Example

### Web: Accessible, semantic, Tailwind

```html
<!-- Correct: Accessible, semantic, responsive -->
<div class="max-w-4xl mx-auto p-6 bg-white dark:bg-gray-900 rounded-lg shadow-lg">
  <h1 class="text-2xl font-bold text-gray-900 dark:text-white mb-4">
    Dashboard
  </h1>
  <button
    class="px-4 py-2 bg-blue-600 text-white rounded hover:bg-blue-700 focus:ring-2 focus:ring-blue-500"
    aria-label="Perform action"
  >
    Action
  </button>
</div>

<!-- Incorrect: Inline styles, no accessibility, no responsiveness -->
<div style="width: 800px; margin: 0 auto;">
  <div style="font-size: 16px;">Dashboard</div>
  <div style="background: blue; color: white; padding: 8px;">Action</div>
</div>
```

### Desktop (PyQt): Proper event handling, clean architecture

```python
# Correct: Clean event loop, non-blocking, proper resource management
import sys
from PyQt5.QtWidgets import QApplication, QWidget, QPushButton, QVBoxLayout

class App(QWidget):
    def __init__(self):
        super().__init__()
        layout = QVBoxLayout()
        btn = QPushButton("Click Me")
        btn.clicked.connect(self.on_click)
        layout.addWidget(btn)
        self.setLayout(layout)
        self.setWindowTitle("My App")

    def on_click(self):
        print("Button clicked!")

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = App()
    window.show()
    sys.exit(app.exec_())

# Incorrect: Blocking, no event handling, frozen UI
while True:
    time.sleep(1)  # Blocks everything
    # No event loop → frozen UI
```

### CSS: GPU-accelerated animations, custom properties

```css
/* Correct: Custom properties, GPU-accelerated */
:root {
  --color-primary: #3b82f6;
  --color-primary-hover: #2563eb;
  --radius: 0.5rem;
}

.btn {
  background: var(--color-primary);
  color: white;
  padding: 0.5rem 1rem;
  border-radius: var(--radius);
  transition: background-color 0.2s ease, transform 0.1s ease;
}

.btn:hover {
  background: var(--color-primary-hover);
  transform: translateY(-1px);
}

/* Incorrect: Hardcoded values, layout-triggering animation */
.btn {
  background: blue;
  padding: 8px 16px;
  animation: move 1s linear infinite; /* Triggers layout */
}

@keyframes move {
  0% { margin-left: 0; }
  100% { margin-left: 100px; }
}
```
