# Release Automation - Requirements Summary

## Current Automation Level ✅

The `scripts/release.sh` script automates:

1. ✅ **Version Bump** - Auto-increments patch version
2. ✅ **Changelog Generation** - Extracts first line from commits between tags
3. ✅ **File Updates** - Updates CMakeLists.txt, src/main.c, debian/changelog
4. ✅ **Package Building** - Builds Debian packages via dpkg-buildpackage
5. ✅ **Git Tagging** - Creates annotated tags with release messages
6. ✅ **GitHub Push** - Pushes commits and tags to origin
7. ✅ **Release Creation** - Creates GitHub release with artifacts using gh CLI
8. ✅ **Artifact Upload** - Uploads .deb, .buildinfo, .changes files
9. ✅ **Color Output** - User-friendly colored terminal output
10. ✅ **Error Handling** - Validates input, checks requirements

## Additional Information for Full CI/CD Automation

To make the process **completely hands-off** (zero manual intervention), you would need:

### 1. GitHub Actions Workflow (Recommended)

**File**: `.github/workflows/release.yml`

```yaml
name: Automated Release
on:
  push:
    tags:
      - 'v*'
jobs:
  build-and-release:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Build and Release
        run: ./scripts/release.sh
        env:
          GH_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```

**Benefits**:
- Automatic release on every git push with tag
- No local build required
- Consistent build environment
- Automatic workflow logging

### 2. Configuration File (Optional)

**File**: `.releaserc.json` or `release.config.js`

```json
{
  "project": {
    "name": "radxa-penta-fan-ctrl",
    "repo": "kYc0o/radxa-penta-sata-hat-top-board-ctrl-c",
    "maintainer": {
      "name": "Francisco Javier Acosta Padilla",
      "email": "fco.ja.ac@gmail.com"
    }
  },
  "versioning": {
    "scheme": "semantic",
    "bump": "patch"
  },
  "build": {
    "targets": ["arm64", "armhf", "amd64"],
    "artifacts": ["*.deb"]
  },
  "changelog": {
    "auto_generate": true,
    "include_commits": true
  }
}
```

### 3. Testing Automation

**File**: `.github/workflows/test.yml`

```yaml
name: Tests
on: [push, pull_request]
jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Build
        run: make -C build
      - name: Check Warnings
        run: make -C build 2>&1 | grep -i "warning" && exit 1 || exit 0
```

### 4. Multi-Architecture Builds

**Requires**:
- Docker buildx setup
- QEMU support for ARM cross-compilation
- Separate build configurations per architecture

```bash
# Would build for multiple architectures
./scripts/release.sh --all-archs
```

### 5. Automated Changelog

**Tool**: `conventional-commits` or `conventional-changelog`

Instead of just first line of commits, parse:
```
commit message format:
feat(area): description       -> Feature
fix(area): description        -> Fix
perf(area): description       -> Performance
docs(area): description       -> Documentation
refactor(area): description   -> Refactoring
```

Generated changelog sections:
```markdown
## Features
- New OLED rotation support

## Fixes
- Fixed config file parsing whitespace issue

## Performance
- Reduced PWM frequency from 25kHz to 500Hz

## Documentation
- Added RELEASING.md guide
```

### 6. Pre-release Checks

Automate verification:
- [ ] All tests passing
- [ ] Code coverage threshold met
- [ ] No compiler warnings
- [ ] Git tree clean
- [ ] Dependencies up to date
- [ ] Security scan passed

### 7. Distribution to Package Managers

**Launchpad PPA** (Ubuntu):
```bash
dput ppa:username/ppa-name ../radxa-penta-fan-ctrl_*.deb
```

**Arch User Repository**:
- Maintain PKGBUILD file
- Automatic updates on release

**Snap Package**:
- snapcraft.yaml configuration
- Automatic snap store publishing

## Current Capabilities vs. What's Needed

| Feature | Current | CI/CD | Cloud Build | Multi-Arch |
|---------|---------|-------|-------------|------------|
| Version bump | ✅ Manual | ✅ Auto | ✅ Auto | ✅ Auto |
| Changelog | ✅ Commit-based | ✅ Conventional | ✅ Conventional | ✅ Conventional |
| Building | ✅ Local | ✅ GitHub | ✅ Cloud | ✅ Multi-target |
| Testing | ❌ None | ✅ Full | ✅ Full | ✅ Per-arch |
| Upload | ✅ Manual | ✅ Auto | ✅ Auto | ✅ Auto all |
| Distribution | ❌ None | ❌ None | ✅ PPA/Snap | ✅ PPA/Snap |

## Recommended Next Steps

### Quick (1-2 hours)
1. ✅ **Release script** - DONE
2. ✅ **Documentation** - DONE
3. Add GitHub Actions for CI validation

### Medium (2-4 hours)
1. GitHub Actions workflow for releases
2. Multi-architecture build support
3. Automated testing on every push

### Advanced (4+ hours)
1. Conventional commit parsing
2. PPA/Snap integration
3. Release notes from GitHub milestones
4. Automated security scanning

## Usage Summary

**Current Workflow** (What we have now):
```bash
# Make commits normally
git add <files>
git commit -m "your changes"

# When ready to release:
./scripts/release.sh 1.0.2  # or just ./scripts/release.sh

# Script handles everything:
# - Version updates
# - Changelog generation
# - Package building
# - Git tagging
# - GitHub release creation
```

**Future Workflow** (With full CI/CD):
```bash
# Make commits with conventional format
git add <files>
git commit -m "feat(oled): add rotation support"

# CI automatically runs tests, no manual build needed

# Create annotated tag
git tag v1.0.2

# Push
git push origin main --tags

# CI/CD automatically:
# - Builds on GitHub Actions
# - Runs tests
# - Creates release
# - Publishes to multiple package managers
# - Updates website
```

## Questions for Implementation

Before implementing CI/CD automation, consider:

1. **Build Target Platforms**: Just arm64? Or also armhf, amd64?
2. **Package Managers**: Just GitHub releases? Or also Ubuntu PPA, Snap Store?
3. **Testing Requirements**: Unit tests? Integration tests? Linting?
4. **Deployment Targets**: Just GitHub? Or also auto-deploy to servers?
5. **Notification**: Slack/Discord notifications on release?
6. **Schedule**: Manual trigger, or automatic on schedule?
7. **Rollback**: Need ability to roll back failed releases?

## Current State

✅ **Fully automated release script**
- No manual version editing needed
- Changelog auto-generated from commits
- One command releases

🟡 **Partially automatable**
- Build happens locally (could be CI)
- Requires local authentication (could use secrets)
- Manual git push needed (could be automatic)

❌ **Not automated**
- Testing (could add GitHub Actions)
- Multi-architecture builds (needs setup)
- Distribution to PPA/Snap (external tools needed)

---

**Recommendation**: Current script is excellent for most workflows. Add GitHub Actions only if you have specific CI/CD requirements.
