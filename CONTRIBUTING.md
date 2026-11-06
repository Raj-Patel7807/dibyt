# Contributing to Dibyt

Thank you for your interest in contributing to **Dibyt**! We appreciate bug reports, feature discussions, documentation improvements, and code contributions.

---

## 1. Code of Conduct

All contributors and maintainers are expected to adhere to our [Code of Conduct](CODE_OF_CONDUCT.md). Please treat all community members with respect and courtesy.

---

## 2. Engineering Standards & Expectations

Before submitting any code changes, review our [Development Standards](docs/development/STANDARDS.md):
- **C++ Standard:** All product code must conform to modern C++20 (`-std=c++20`).
- **Safety & RAII:** Manage all resources (sockets, memory, file descriptors, SQLite handles) via RAII wrappers.
- **Bounded Resources:** Avoid unbounded threads, queues, allocations, or retries.
- **No Direct Push to `main`:** All external contributions must arrive via pull requests that pass automated CI checks.
- **Commit Messages:** Write clear, concise, senior-grade commit messages. Never reference AI assistants, tools, or ephemeral prompts.

---

## 3. Pull Request Process

1. Fork the repository and create your branch from `main`.
2. Ensure new code includes accompanying unit or integration tests (`docs/development/TESTING.md`).
3. Verify that all tests pass locally and code formatting conforms to repository rules.
4. Update corresponding documentation in `docs/` and add Architecture Decision Records (`docs/decisions/`) for any non-trivial structural decisions.
5. Submit the pull request with a clear description of the problem solved and test evidence provided.
