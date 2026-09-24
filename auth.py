# app/auth.py
#
# Internal auth for the Phase 5 -> Phase 6 handoff (spec §19A.3, §19A.6).
# This is NOT user authentication — Phase 2 already owns that, and identity
# arrives pre-verified inside OrchestratorContext.identity. This check only
# confirms the CALLER is really Phase 5 (a shared-secret service token),
# using constant-time comparison to avoid timing side-channels.
from secrets import compare_digest

from fastapi import Request

from .config import settings
from .errors import UnauthenticatedError


def require_internal_auth(request: Request) -> None:
    configured = settings.internal_service_token
    supplied = request.headers.get("X-Internal-Auth")

    if not configured:
        # Fail closed: an empty configured token must never mean "accept
        # anything". This mirrors Phase 4's require_phase1_internal pattern.
        raise UnauthenticatedError("Internal service token is not configured")

    if not supplied or not compare_digest(supplied, configured):
        raise UnauthenticatedError()
