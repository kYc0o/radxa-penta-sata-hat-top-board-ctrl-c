# Release Process Guide

This document describes how to create and publish a new release of the Radxa Penta Fan Controller.

## Automated Release Process

We provide a release automation script that handles the entire release workflow:

```bash
./scripts/release.sh [VERSION]
```

### Prerequisites

Before running the release script, ensure:

1. **Git Configuration**: Commits must be properly attributed
   ```bash
   git config user.name "Your Name"
   git config user.email "your.email@example.com"
   ```

2. **SSH Key Setup**: Required for pushing to GitHub
   ```bash
   ssh-add ~/.ssh/id_rsa  # or your SSH key
   ```

3. **GitHub CLI Authentication**: Required for creating releases
   ```bash
   gh auth login
   ```

4. **Build Dependencies**: Required for building Debian packages
   ```bash
   sudo apt-get install build-essential debhelper dpkg-dev
   ```

5. **Clean Working Directory**: All changes must be committed
   ```bash
   git status  # Should show "working tree clean"
   ```

## Release Workflow

### Quick Release (Interactive)

Run the script without arguments for an interactive walkthrough:

```bash
./scripts/release.sh
```

The script will:
1. Suggest the next patch version (auto-incremented)
2. Prompt for release title and description
3. Extract commits since the last release
4. Update all version numbers
5. Build Debian packages
6. Create git tag
7. Push to GitHub
8. Create GitHub release with artifacts

### Direct Release (Non-interactive)

Specify the version directly to skip interactive prompts:

```bash
./scripts/release.sh 1.0.2
```

## What the Script Does

### 1. Version Updates

The script updates version numbers in:
- `CMakeLists.txt` - CMake project version
- `src/main.c` - Application version string
- `debian/changelog` - Debian changelog

### 2. Changelog Generation

Automatically generates changelog entries from:
- Commit messages between the last tag and HEAD
- First line of each commit becomes a changelog entry
- Can be overridden with custom release description

Example generated changelog:
```
radxa-penta-fan-ctrl (1.0.2-1) unstable; urgency=medium

  - thermal: optimise algorithm and reduce SSD info polling
  - fan: reduce soft PWM frequency and optimise fan control
  - Fix config file parsing: properly trim whitespace around values

 -- Francisco Javier Acosta Padilla <fco.ja.ac@gmail.com>  Date: ...
```

### 3. Package Building

Builds Debian packages:
- `radxa-penta-fan-ctrl_X.Y.Z-1_arm64.deb` - Main package
- `radxa-penta-fan-ctrl-dbgsym_X.Y.Z-1_arm64.deb` - Debug symbols
- Supporting files (.buildinfo, .changes)

### 4. Git Tag Creation

Creates an annotated git tag:
```bash
git tag -a vX.Y.Z -m "Release Title"
```

### 5. GitHub Release

Publishes a release with:
- Release title and notes
- All Debian package artifacts
- Automatically generated release notes (if not provided)
- SHA256 checksums

## Information Required for Full Automation

To make the release process completely automated, you would need:

1. **Configuration File** (optional)
   - Project name
   - Maintainer name and email
   - GitHub repository
   - Build target (arm64, armhf, etc.)

2. **CI/CD Integration** (optional)
   - Automatically run tests before release
   - Build for multiple architectures
   - Automatic changelog generation

3. **Versioning Conventions**
   - Currently: Semantic versioning (X.Y.Z)
   - Auto-increment patch version by default

4. **Release Notes Template** (optional)
   - Custom template for consistent formatting
   - Sections for features, fixes, improvements

## Version Numbering

We follow [Semantic Versioning](https://semver.org/):

- **MAJOR** (X.0.0): Breaking changes, incompatible updates
- **MINOR** (0.Y.0): New features, backward compatible
- **PATCH** (0.0.Z): Bug fixes, optimizations

Examples:
- `1.0.0` - Initial release
- `1.0.1` - Bug fixes, optimizations
- `1.1.0` - New features added
- `2.0.0` - Major changes, breaking changes

## Manual Release (If Script Fails)

If the automated script encounters issues, you can release manually:

### 1. Update Version Files

```bash
# Edit CMakeLists.txt
sed -i 's/VERSION 1.0.1/VERSION 1.0.2/' CMakeLists.txt

# Edit src/main.c
sed -i 's/v1.0.1/v1.0.2/' src/main.c

# Edit debian/changelog (manually add entry at top)
nano debian/changelog
```

### 2. Build Package

```bash
cd /path/to/repo
dpkg-buildpackage -b -uc -us
```

### 3. Create Git Tag

```bash
git add debian/changelog CMakeLists.txt src/main.c
git commit -m "Bump version to 1.0.2"
git tag -a v1.0.2 -m "Release v1.0.2"
```

### 4. Push to GitHub

```bash
git push origin main
git push origin v1.0.2
```

### 5. Create GitHub Release

```bash
gh release create v1.0.2 \
  ../radxa-penta-fan-ctrl_1.0.2-1_arm64.deb \
  ../radxa-penta-fan-ctrl-dbgsym_1.0.2-1_arm64.deb \
  --title "v1.0.2: Release Title" \
  --notes "Release notes here"
```

## Troubleshooting

### SSH Key Issues

**Error**: `Permission denied (publickey)`

**Solution**:
```bash
ssh-add ~/.ssh/id_rsa
ssh -T git@github.com  # Test connection
```

### GitHub CLI Token Issues

**Error**: `HTTP 403: Resource not accessible by personal access token`

**Solution**:
1. Verify token has `repo` scope
2. Re-authenticate: `gh auth logout` then `gh auth login`
3. Or use SSH push instead of HTTPS

### Build Failures

**Error**: `dpkg-buildpackage: error`

**Solution**:
```bash
# Clean build artifacts
rm -rf build obj-aarch64-linux-gnu debian/radxa-penta-fan-ctrl

# Rebuild from scratch
cmake -B build
make -C build
dpkg-buildpackage -b -uc -us
```

### Tag Already Exists

**Error**: `fatal: tag 'v1.0.1' already exists`

**Solution**: Use different version number or delete tag:
```bash
git tag -d v1.0.1  # Local deletion
git push origin :v1.0.1  # Remote deletion
```

## Release Checklist

Before releasing, verify:

- [ ] All changes committed: `git status` shows clean working tree
- [ ] Code compiles: `make -C build`
- [ ] No compiler warnings
- [ ] Tests pass (if applicable)
- [ ] Changelog updated with clear descriptions
- [ ] Version bumped in all files
- [ ] Git SSH access working: `ssh -T git@github.com`
- [ ] GitHub CLI authenticated: `gh auth status`

## Automation Opportunities

Possible future improvements:

1. **CI/CD Pipeline** (GitHub Actions)
   - Automatic testing on every commit
   - Build packages on tag creation
   - Publish releases automatically

2. **Semantic Release Tools**
   - Automatic version bumping
   - Conventional commit parsing
   - Auto-generated release notes

3. **Multi-Architecture Builds**
   - Build for armhf, arm64, amd64
   - Cross-compile support

4. **Docker Build Environment**
   - Reproducible builds
   - Consistent dependencies

Example GitHub Actions workflow:

```yaml
name: Release
on:
  push:
    tags:
      - 'v*'

jobs:
  release:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Build Packages
        run: dpkg-buildpackage -b -uc -us
      - name: Create Release
        run: gh release create ...
```

## Questions?

For questions or issues with the release process, refer to:
- GitHub Issues: https://github.com/kYc0o/radxa-penta-sata-hat-top-board-ctrl-c/issues
- Git documentation: https://git-scm.com/doc
