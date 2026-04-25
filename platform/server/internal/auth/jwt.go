// Package auth signs and verifies JWTs for the platform. RS256 so the
// public key can be distributed to the relay service for ticket
// verification without exposing the signing key.
package auth

import (
	"crypto/rsa"
	"errors"
	"fmt"
	"os"
	"time"

	"github.com/golang-jwt/jwt/v5"
)

type Signer struct {
	priv *rsa.PrivateKey
	pub  *rsa.PublicKey
	issuer string
	accessTTL  time.Duration
	refreshTTL time.Duration
}

type Claims struct {
	UID int64  `json:"uid"`
	TokenType string `json:"typ"`   // "access" | "refresh"
	jwt.RegisteredClaims
}

func NewSigner(privPath, pubPath, issuer string, accessTTL, refreshTTL time.Duration) (*Signer, error) {
	privBytes, err := os.ReadFile(privPath)
	if err != nil { return nil, fmt.Errorf("read priv key: %w", err) }
	priv, err := jwt.ParseRSAPrivateKeyFromPEM(privBytes)
	if err != nil { return nil, fmt.Errorf("parse priv key: %w", err) }

	pubBytes, err := os.ReadFile(pubPath)
	if err != nil { return nil, fmt.Errorf("read pub key: %w", err) }
	pub, err := jwt.ParseRSAPublicKeyFromPEM(pubBytes)
	if err != nil { return nil, fmt.Errorf("parse pub key: %w", err) }

	return &Signer{
		priv: priv, pub: pub, issuer: issuer,
		accessTTL: accessTTL, refreshTTL: refreshTTL,
	}, nil
}

func (s *Signer) SignAccess(uid int64) (string, error) {
	return s.sign(uid, "access", s.accessTTL)
}

func (s *Signer) SignRefresh(uid int64) (string, error) {
	return s.sign(uid, "refresh", s.refreshTTL)
}

func (s *Signer) sign(uid int64, typ string, ttl time.Duration) (string, error) {
	now := time.Now().UTC()
	claims := Claims{
		UID: uid,
		TokenType: typ,
		RegisteredClaims: jwt.RegisteredClaims{
			Issuer:    s.issuer,
			Subject:   fmt.Sprintf("%d", uid),
			IssuedAt:  jwt.NewNumericDate(now),
			NotBefore: jwt.NewNumericDate(now),
			ExpiresAt: jwt.NewNumericDate(now.Add(ttl)),
			ID:        jwt.NewNumericDate(now).Time.Format("20060102150405.000000000"),
		},
	}
	token := jwt.NewWithClaims(jwt.SigningMethodRS256, claims)
	return token.SignedString(s.priv)
}

func (s *Signer) Verify(tokenStr string) (*Claims, error) {
	c := &Claims{}
	_, err := jwt.ParseWithClaims(tokenStr, c, func(t *jwt.Token) (interface{}, error) {
		if _, ok := t.Method.(*jwt.SigningMethodRSA); !ok {
			return nil, errors.New("unexpected signing method")
		}
		return s.pub, nil
	})
	if err != nil { return nil, err }
	return c, nil
}
