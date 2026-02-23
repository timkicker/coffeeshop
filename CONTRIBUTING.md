# Contributing to CupStore

Thanks for your interest. Here's how to get started.

## Setup

Requirements: devkitPro with WUT, SDL2, SDL2_ttf, SDL2_image, libcurl, cmake.

```bash
git clone https://github.com/timkicker/cupstore
cd cupstore
./build_and_run.sh        # builds and runs in Cemu
./build_and_run.sh hw     # hardware build
```

Tests (no devkitPro needed):
```bash
cd tests && mkdir -p build && cd build
cmake .. && make && ./cupstore_tests
```

## Workflow

- `main`: stable, released builds only
- `dev`: active development, PRs go here

1. Fork and create a branch from `dev`
2. Make your changes
3. Run tests: `cd tests/build && make && ./cupstore_tests`
4. Open a PR against `dev`

PRs against `main` are only for hotfixes.

## Code Style

- C++17, no exceptions in UI code
- Error handling: log + graceful fallback, never crash
- New features that touch the repo format: update `validate.py` in cupstore-repo-template too

## Repo Format Changes

If your change affects how `repo.json` or `game.json` is parsed, please also open a PR in [cupstore-repo-template](https://github.com/timkicker/cupstore-repo-template) to update the schema docs and example files.

## License

By contributing you agree your code is licensed under [GPLv3](LICENSE).
