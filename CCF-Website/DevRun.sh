#!/bin/bash
# INTERNAL_URL points the header "Login" button at the local API (:8080) during
# dev. Prod builds (Build.sh) leave it unset, so the button uses the same-domain
# /internal path instead.
INTERNAL_URL="${INTERNAL_URL:-http://localhost:8080/internal}" mkdocs serve --dev-addr localhost:9000 --livereload
