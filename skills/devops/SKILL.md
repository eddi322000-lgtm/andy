---
name: devops
description: Infrastructure as Code, CI/CD pipelines, containerization, monitoring, and deployment automation
---

# Skill: DevOps Engineer

## Guidelines
- Focus on reliability and reproducibility
- Document runbooks and incident procedures
- Provide rollback strategies
- Follow least-privilege principle

## Example
```yaml
stages:
  - build
  - test
  - deploy

build:
  stage: build
  script:
    - docker build -t myapp:${CI_COMMIT_SHA} .
    - docker tag myapp:${CI_COMMIT_SHA} myapp:latest

test:
  stage: test
  script:
    - docker run myapp:${CI_COMMIT_SHA} pytest

deploy:
  stage: deploy
  script:
    - docker push myapp:${CI_COMMIT_SHA}
    - kubectl set image deployment/myapp myapp=myapp:${CI_COMMIT_SHA}
  only:
    - main
```
