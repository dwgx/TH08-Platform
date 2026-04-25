package httpapi

import (
	"context"
	"net/http"
	"strings"

	"github.com/dwgx/th-platform/server/internal/auth"
)

type ctxKey string

const (
	ctxKeyClaims ctxKey = "claims"
	ctxKeyUID    ctxKey = "uid"
)

// RequireAuth validates the Authorization bearer token against the
// signer and stashes claims + uid into the request context. Endpoints
// downstream read them via ClaimsFromCtx / UIDFromCtx.
func RequireAuth(signer *auth.Signer) func(http.Handler) http.Handler {
	return func(next http.Handler) http.Handler {
		return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			token := extractBearerToken(r)
			if token == "" {
				WriteError(w, ErrAuthRequired)
				return
			}
			claims, err := signer.Verify(token)
			if err != nil || claims.TokenType != "access" {
				WriteError(w, ErrInvalidToken)
				return
			}
			ctx := context.WithValue(r.Context(), ctxKeyClaims, claims)
			ctx = context.WithValue(ctx, ctxKeyUID, claims.UID)
			next.ServeHTTP(w, r.WithContext(ctx))
		})
	}
}

func extractBearerToken(r *http.Request) string {
	h := r.Header.Get("Authorization")
	if h == "" { return "" }
	parts := strings.SplitN(h, " ", 2)
	if len(parts) != 2 || !strings.EqualFold(parts[0], "Bearer") { return "" }
	return strings.TrimSpace(parts[1])
}

func UIDFromCtx(ctx context.Context) (int64, bool) {
	v, ok := ctx.Value(ctxKeyUID).(int64)
	return v, ok
}

func ClaimsFromCtx(ctx context.Context) (*auth.Claims, bool) {
	v, ok := ctx.Value(ctxKeyClaims).(*auth.Claims)
	return v, ok
}
