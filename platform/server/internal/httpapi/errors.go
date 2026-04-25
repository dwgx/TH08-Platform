// Package httpapi is the outer HTTP layer: routing, middleware, request/
// response shaping, and error -> HTTP mapping. Business logic lives in
// the respective internal/<service> packages.
package httpapi

import (
	"encoding/json"
	"errors"
	"net/http"
)

// APIError is the shape returned to clients on any 4xx/5xx.
type APIError struct {
	HTTPStatus int                    `json:"-"`
	Code       string                 `json:"code"`
	Message    string                 `json:"message"`
	Detail     map[string]any         `json:"detail,omitempty"`
	RequestID  string                 `json:"request_id,omitempty"`
}

func (e *APIError) Error() string { return e.Code + ": " + e.Message }

var (
	ErrAuthRequired      = &APIError{HTTPStatus: 401, Code: "auth_required",      Message: "Authentication required."}
	ErrInvalidToken      = &APIError{HTTPStatus: 401, Code: "token_expired",      Message: "Access token expired or invalid."}
	ErrForbidden         = &APIError{HTTPStatus: 403, Code: "forbidden",          Message: "You are not allowed to do this."}
	ErrNotFound          = &APIError{HTTPStatus: 404, Code: "not_found",          Message: "Resource not found."}
	ErrHandleTaken       = &APIError{HTTPStatus: 409, Code: "handle_taken",       Message: "This handle is already in use."}
	ErrEmailTaken        = &APIError{HTTPStatus: 409, Code: "email_taken",        Message: "This email is already registered."}
	ErrValidationFailed  = &APIError{HTTPStatus: 400, Code: "validation_failed",  Message: "Request validation failed."}
	ErrRateLimited       = &APIError{HTTPStatus: 429, Code: "rate_limited",       Message: "You are sending requests too quickly."}
	ErrInternal          = &APIError{HTTPStatus: 500, Code: "internal_error",     Message: "Something went wrong. Please try again."}
)

// WriteError renders an APIError (or wraps a generic error as 500).
func WriteError(w http.ResponseWriter, err error) {
	var ae *APIError
	if errors.As(err, &ae) {
		writeJSON(w, ae.HTTPStatus, map[string]any{"error": ae})
		return
	}
	writeJSON(w, http.StatusInternalServerError, map[string]any{
		"error": ErrInternal,
	})
}

func writeJSON(w http.ResponseWriter, status int, body any) {
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	w.WriteHeader(status)
	_ = json.NewEncoder(w).Encode(body)
}

// WriteOK is the happy-path counterpart.
func WriteOK(w http.ResponseWriter, body any) {
	writeJSON(w, http.StatusOK, body)
}

// WriteCreated writes a 201.
func WriteCreated(w http.ResponseWriter, body any) {
	writeJSON(w, http.StatusCreated, body)
}
