#!/bin/bash -e

# Settings
REPO_PATH="https://rayslava:${GIT_TOKEN}@github.com/rayslava/chatsync.git"
HTML_PATH=doc/html
COMMIT_USER="Documentation Builder"
COMMIT_EMAIL="travis@travis-ci.org"
CHANGESET=$(git rev-parse --verify HEAD)

# Get a clean version of the HTML documentation repo.
rm -rf ${HTML_PATH}
mkdir -p ${HTML_PATH}
git clone -b gh-pages "${REPO_PATH}" --single-branch ${HTML_PATH}
set -x

# Prepare configuration and README file
sed -ie "/PROJECT_NUMBER/s/=.*$/= ${CHANGESET}/" Doxyfile
sed -ie '1s/$/ {#mainpage}/' README.md
sed -ie '/Build Status/s#.*#!\[Build Status\](https://travis-ci.org/rayslava/chatsync.svg?branch=master) !\[Coverage Status\](https://coveralls.io/repos/rayslava/chatsync/badge.png)#' README.md

# rm all the files through git to prevent stale files.
cd ${HTML_PATH}
git rm -rf .
cd -

# Generate the HTML documentation.
make doxygen

# Create and commit the documentation repo.
cd ${HTML_PATH}
git add .
git config user.name "${COMMIT_USER}"
git config user.email "${COMMIT_EMAIL}"
git commit -m "Automated documentation build for changeset ${CHANGESET}."
git push origin gh-pages
cd -

