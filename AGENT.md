# Agent Guidelines — Dibyt

This document defines the development guidelines for Dibyt. All developers and AI coding agents must follow these rules when working on the project.

## 1. Understand Before Making Changes

- Read `README.md` and relevant documentation before starting work.
- Understand the existing project structure and implementation before modifying anything.
- Check the current Git status and preserve existing user changes.
- Never assume a file is unnecessary or safe to overwrite.
- Follow the current task's scope. Do not implement unrelated features or future roadmap items.

## 2. Engineering Principles

- Prefer simple, clean, readable, and maintainable code.
- Follow modular architecture and keep each module responsible for one clear task.
- Avoid unnecessary abstractions, duplicated logic, and oversized source files.
- Do not introduce dependencies without a good reason.
- Use meaningful names and clear interfaces.
- Add comments only when they explain important decisions or non-obvious logic.
- Do not leave incomplete implementations, fake responses, or unnecessary TODOs.

## 3. C++ Development

- Follow the C++ standard and conventions established by the project.
- Use modern C++ practices, RAII, and proper resource management.
- Handle errors explicitly; never silently ignore failures.
- Avoid unnecessary global state, raw owning pointers, and unsafe memory management.
- Keep networking, protocol, transfer, scheduling, storage, and application logic properly separated.

## 4. Networking and File Transfer

- Remember that TCP is a byte stream. Never assume one read or write transfers the complete requested data.
- Handle partial reads, partial writes, disconnections, timeouts, and network failures correctly.
- Validate incoming messages, protocol fields, and payload sizes.
- Keep the transfer protocol versioned and update its documentation when it changes.
- Support arbitrary binary files; never rely on the original coursework dataset or its assumptions.
- Verify transferred files using SHA-256 before marking them complete.
- Implement checkpoints and recovery carefully so that interrupted transfers can resume without corrupting data.
- Use bounded workers, queues, buffers, and connections. Avoid uncontrolled resource consumption.

## 5. Security and Reliability

- Treat all network input, file paths, and external configuration as untrusted.
- Prevent path traversal and unauthorized filesystem access.
- Never expose files outside the configured storage and destination directories.
- Do not hardcode credentials, tokens, or private keys.
- Never commit secrets or log sensitive information.
- Use proper authentication and TLS for protected communication.
- Handle failures without losing valid transfer state or corrupting existing files.
- Prefer safe defaults and explicit limits.

## 6. Architecture and Documentation

- Keep Dibyt as a modular monolith unless there is a strong reason to change that architecture.
- Keep bulk file transfer on the dedicated TCP protocol; use the HTTP API for management and monitoring.
- Keep the CLI, server, API, and future dashboard separate from reusable core logic.
- Update relevant documentation whenever architecture, protocol, configuration, APIs, or behavior changes.
- Clearly distinguish implemented features from planned features.
- Record important architectural decisions in `docs/decisions/`.

## 7. Testing and Code Quality

- Add or update tests for every meaningful behavior change.
- Test both successful and failure scenarios.
- Pay special attention to interrupted transfers, corrupted data, concurrent clients, worker failures, and server restarts.
- Run the relevant build, formatting, static analysis, and tests before completing a task.
- Never remove or weaken tests just to make the build pass.
- Never claim that a test or build passed unless it was actually executed successfully.

## 8. Git Workflow

- Work directly on the `main` branch.
- Do not create branches unless explicitly requested.
- Do not commit or push unless explicitly requested.
- Never force-push, reset, clean, or discard existing changes without permission.
- Keep changes focused and reviewable.
- Preserve the original coursework implementations, reports, and benchmark results under `legacy/academic/`.

## 9. Working with AI Agents

Before implementing a task:

1. Understand the task and its acceptance criteria.
2. Inspect the relevant code, documentation, and tests.
3. Identify the files and modules that need changes.
4. Make a small implementation plan.
5. Implement only the requested scope.
6. Run relevant tests and checks.
7. Review the final diff for accidental or unrelated changes.
8. Summarize what changed, what was tested, and any remaining limitations.

If requirements are unclear or conflict with existing architecture, explain the issue instead of making assumptions.

**Most importantly: Build Dibyt for correctness, reliability, security, and maintainability. Do not sacrifice these qualities just to finish a task faster.**
