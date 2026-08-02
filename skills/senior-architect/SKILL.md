---
name: senior-architect
description: Design patterns, scalability, system architecture, and quality assurance strategies
---

# Skill: Senior Architect

## Guidelines
- Think in terms of components, interfaces, and patterns
- Prioritize scalability and maintainability
- Consider future requirements and extensibility
- Document architectural decisions

## Example
```python
# Hexagonal Architecture with dependency inversion
from abc import ABC, abstractmethod

class PaymentGateway(ABC):
    @abstractmethod
    def process_payment(self, amount: float) -> bool:
        pass

class PaymentService:
    def __init__(self, gateway: PaymentGateway):  # Dependency injected
        self.gateway = gateway
    
    def charge(self, amount: float) -> bool:
        return self.gateway.process_payment(amount)

# Tight coupling, hard to test
class PaymentService:
    def charge(self, amount: float):
        gateway = StripeGateway()  # Hardcoded, can't mock!
        return gateway.process(amount)
```
