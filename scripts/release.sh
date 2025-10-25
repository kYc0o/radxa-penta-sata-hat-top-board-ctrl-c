#!/bin/bash

################################################################################
# Radxa Penta Fan Controller - Release Automation Script
#
# This script automates the release process:
# 1. Extracts commit messages between the last two tags
# 2. Prompts for release information (version, description)
# 3. Updates changelog, CMakeLists.txt, and version strings
# 4. Builds Debian packages
# 5. Creates git tag and GitHub release with artifacts
#
# Usage: ./scripts/release.sh [VERSION]
# Example: ./scripts/release.sh 1.0.2
################################################################################

set -e

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
PROJECT_NAME="radxa-penta-fan-ctrl"
MAINTAINER_NAME="Francisco Javier Acosta Padilla"
MAINTAINER_EMAIL="fco.ja.ac@gmail.com"
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEBIAN_DIR="$REPO_ROOT/debian"
CMAKE_FILE="$REPO_ROOT/CMakeLists.txt"
MAIN_FILE="$REPO_ROOT/src/main.c"
CHANGELOG_FILE="$DEBIAN_DIR/changelog"

################################################################################
# Utility Functions
################################################################################

log_info() {
    echo -e "${BLUE}ℹ${NC} $1"
}

log_success() {
    echo -e "${GREEN}✓${NC} $1"
}

log_error() {
    echo -e "${RED}✗${NC} $1" >&2
}

log_warning() {
    echo -e "${YELLOW}⚠${NC} $1"
}

# Get the last release tag
get_last_tag() {
    git describe --tags --abbrev=0 2>/dev/null || echo ""
}

# Get current version from CMakeLists.txt
get_current_version() {
    grep "VERSION" "$CMAKE_FILE" | grep "project(" | sed -E 's/.*VERSION ([0-9.]+).*/\1/'
}

# Extract commit messages between two tags
get_commits_between_tags() {
    local from_tag=$1
    local to_tag=$2

    if [ -z "$from_tag" ]; then
        # If no previous tag, get all commits
        git log --oneline "$to_tag" | cut -d' ' -f2-
    else
        git log --oneline "$from_tag..$to_tag" | cut -d' ' -f2-
    fi
}

# Format commits as changelog entries
format_changelog_entries() {
    local commits="$1"
    echo "$commits" | while read -r commit; do
        echo "    - $commit"
    done
}

# Validate version format
validate_version() {
    local version=$1
    if [[ ! $version =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
        log_error "Invalid version format: $version"
        log_info "Version must be in format: X.Y.Z (e.g., 1.0.2)"
        return 1
    fi
    return 0
}

################################################################################
# Main Functions
################################################################################

# Interactive release setup
setup_release() {
    local suggested_version=$1

    echo -e "\n${BLUE}=== Radxa Penta Fan Controller Release Setup ===${NC}\n"

    log_info "Current version: $(get_current_version)"

    # Get version
    while true; do
        read -p "Enter new version [$suggested_version]: " new_version
        new_version=${new_version:-$suggested_version}

        if validate_version "$new_version"; then
            break
        fi
    done

    # Get release notes
    log_info "Enter release title (leave blank for default)"
    read -p "> " release_title
    release_title=${release_title:-"Release v$new_version"}

    log_info "Enter release description or leave blank to auto-generate from commits"
    read -p "> " release_description

    # Get last tag for commit extraction
    local last_tag=$(get_last_tag)
    if [ -z "$last_tag" ]; then
        log_warning "No previous release tags found"
    fi

    echo -e "\nVersion: $new_version"
    echo "Title: $release_title"
    [ -z "$release_description" ] && log_info "Description will be auto-generated from commits" || echo "Description: $release_description"

    read -p "Continue? (y/n): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        log_error "Release cancelled"
        return 1
    fi

    echo "$new_version|$release_title|$release_description|$last_tag"
}

# Update version in files
update_version() {
    local new_version=$1

    log_info "Updating version to $new_version..."

    # Update CMakeLists.txt
    sed -i "s/VERSION [0-9.]\+/VERSION $new_version/" "$CMAKE_FILE"
    log_success "Updated CMakeLists.txt"

    # Update main.c version string
    sed -i "s/v[0-9.]\+/v$new_version/" "$MAIN_FILE"
    log_success "Updated src/main.c"
}

# Update changelog
update_changelog() {
    local new_version=$1
    local last_tag=$2
    local release_date=$(date '+%a, %d %b %Y %T %z')

    log_info "Updating debian/changelog..."

    # Get commits for changelog
    local commits=$(get_commits_between_tags "$last_tag" "HEAD")

    if [ -z "$commits" ]; then
        log_warning "No commits found between $last_tag and HEAD"
        local changelog_entries="    * Release v$new_version"
    else
        local changelog_entries=$(format_changelog_entries "$commits")
    fi

    # Create new changelog entry
    local new_entry="${PROJECT_NAME} ($new_version-1) unstable; urgency=medium

$changelog_entries

 -- $MAINTAINER_NAME <$MAINTAINER_EMAIL>  $release_date
"

    # Prepend to changelog
    echo "$new_entry" > "${CHANGELOG_FILE}.tmp"
    cat "$CHANGELOG_FILE" >> "${CHANGELOG_FILE}.tmp"
    mv "${CHANGELOG_FILE}.tmp" "$CHANGELOG_FILE"

    log_success "Updated debian/changelog"
}

# Build Debian package
build_package() {
    log_info "Building Debian package..."

    cd "$REPO_ROOT"
    dpkg-buildpackage -b -uc -us 2>&1 | grep -E "(Building|building|Built|^Err|^error)" || true

    log_success "Package built successfully"
}

# Verify package files exist
verify_artifacts() {
    local version=$1
    local deb_file="${REPO_ROOT}/../${PROJECT_NAME}_${version}-1_arm64.deb"
    local dbgsym_file="${REPO_ROOT}/../${PROJECT_NAME}-dbgsym_${version}-1_arm64.deb"

    if [ ! -f "$deb_file" ]; then
        log_error "Main package not found: $deb_file"
        return 1
    fi

    if [ ! -f "$dbgsym_file" ]; then
        log_error "Debug symbols package not found: $dbgsym_file"
        return 1
    fi

    log_success "Artifacts verified"
    return 0
}

# Commit changes
commit_changes() {
    local new_version=$1

    log_info "Committing version bump..."

    cd "$REPO_ROOT"
    git add debian/changelog CMakeLists.txt src/main.c
    git commit -m "Bump version to $new_version and update changelog" || true

    log_success "Changes committed"
}

# Create git tag
create_tag() {
    local new_version=$1
    local release_title=$2

    log_info "Creating git tag v$new_version..."

    cd "$REPO_ROOT"

    # Check if tag already exists
    if git rev-parse "v$new_version" >/dev/null 2>&1; then
        log_warning "Tag v$new_version already exists, skipping tag creation"
        return 0
    fi

    git tag -a "v$new_version" -m "$release_title"
    log_success "Git tag created"
}

# Push to GitHub
push_to_github() {
    local new_version=$1

    log_info "Pushing to GitHub..."

    cd "$REPO_ROOT"

    # Push main branch
    git push origin main 2>&1 | grep -E "(Total|done|rejected|error)" || true

    # Push tag
    git push origin "v$new_version" 2>&1 | grep -E "(new tag|rejected|error)" || true

    log_success "Pushed to GitHub"
}

# Create GitHub release
create_github_release() {
    local new_version=$1
    local release_title=$2
    local release_description=$3
    local last_tag=$4

    log_info "Creating GitHub release..."

    cd "$REPO_ROOT"

    local deb_file="${REPO_ROOT}/../${PROJECT_NAME}_${new_version}-1_arm64.deb"
    local dbgsym_file="${REPO_ROOT}/../${PROJECT_NAME}-dbgsym_${new_version}-1_arm64.deb"
    local buildinfo_file="${REPO_ROOT}/../${PROJECT_NAME}_${new_version}-1_arm64.buildinfo"
    local changes_file="${REPO_ROOT}/../${PROJECT_NAME}_${new_version}-1_arm64.changes"

    # Build release notes
    local notes=""
    if [ -z "$release_description" ]; then
        # Auto-generate from commits
        local commits=$(get_commits_between_tags "$last_tag" "HEAD")
        notes="## Changes

"
        echo "$commits" | while read -r commit; do
            notes+="- $commit
"
        done
        notes+="
## Installation

\`\`\`bash
sudo dpkg -i ${PROJECT_NAME}_${new_version}-1_arm64.deb
\`\`\`"
    else
        notes="$release_description"
    fi

    # Create release with gh CLI
    if command -v gh &> /dev/null; then
        gh release create "v$new_version" \
            "$deb_file" \
            "$dbgsym_file" \
            "$buildinfo_file" \
            "$changes_file" \
            --title "$release_title" \
            --notes "$notes" 2>&1 | grep -E "(https://|error|Error)" || true

        log_success "GitHub release created"
    else
        log_warning "GitHub CLI (gh) not found. Release must be created manually."
        log_info "Visit: https://github.com/kYc0o/radxa-penta-sata-hat-top-board-ctrl-c/releases"
    fi
}

# Main workflow
main() {
    local suggested_version=$1

    # Handle help flag
    if [ "$suggested_version" = "-h" ] || [ "$suggested_version" = "--help" ]; then
        show_help
        exit 0
    fi

    # If version provided as argument, use it directly
    if [ -n "$suggested_version" ]; then
        if ! validate_version "$suggested_version"; then
            exit 1
        fi
    else
        # Auto-suggest next patch version
        local current=$(get_current_version)
        local major=$(echo $current | cut -d. -f1)
        local minor=$(echo $current | cut -d. -f2)
        local patch=$(echo $current | cut -d. -f3)
        suggested_version="$major.$minor.$((patch + 1))"
    fi

    # Setup release
    local release_info=$(setup_release "$suggested_version") || exit 1

    local new_version=$(echo "$release_info" | cut -d'|' -f1)
    local release_title=$(echo "$release_info" | cut -d'|' -f2)
    local release_description=$(echo "$release_info" | cut -d'|' -f3)
    local last_tag=$(echo "$release_info" | cut -d'|' -f4)

    # Execute release steps
    update_version "$new_version"
    update_changelog "$new_version" "$last_tag"
    commit_changes "$new_version"
    build_package
    verify_artifacts "$new_version" || exit 1
    create_tag "$new_version" "$release_title"
    push_to_github "$new_version"
    create_github_release "$new_version" "$release_title" "$release_description" "$last_tag"

    echo -e "\n${GREEN}✓ Release v$new_version completed successfully!${NC}\n"
    echo "Release URL: https://github.com/kYc0o/radxa-penta-sata-hat-top-board-ctrl-c/releases/tag/v$new_version"
}

# Show help
show_help() {
    cat << 'EOF'
Radxa Penta Fan Controller - Release Automation Script

USAGE:
    ./scripts/release.sh [OPTIONS] [VERSION]

OPTIONS:
    -h, --help      Show this help message
    VERSION         Release version (e.g., 1.0.2)
                    If omitted, next patch version is suggested

EXAMPLES:
    # Interactive release (suggested version)
    ./scripts/release.sh

    # Release specific version
    ./scripts/release.sh 1.0.2

    # Show this help
    ./scripts/release.sh --help

PREREQUISITES:
    - Git configured: git config user.name && git config user.email
    - SSH key added: ssh-add ~/.ssh/id_rsa
    - GitHub CLI authenticated: gh auth login
    - Clean git status: git status (should show no changes)
    - Build tools installed: dpkg-buildpackage, cmake, make

WHAT IT DOES:
    1. Updates version in CMakeLists.txt and src/main.c
    2. Generates changelog from commit messages
    3. Builds Debian packages (.deb files)
    4. Creates git tag and pushes to GitHub
    5. Creates GitHub release with artifacts

RELEASE CHECKLIST:
    ✓ All changes committed
    ✓ Working tree clean (git status)
    ✓ SSH access configured
    ✓ GitHub CLI authenticated
    ✓ No uncommitted changes

For more information, see RELEASING.md
EOF
}
################################################################################
# Entry Point
################################################################################

# Check requirements
if ! command -v git &> /dev/null; then
    log_error "git not found"
    exit 1
fi

if ! command -v dpkg-buildpackage &> /dev/null; then
    log_error "dpkg-buildpackage not found"
    exit 1
fi

# Run main
main "$@"
