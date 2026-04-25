package auth

import (
	"github.com/alexedwards/argon2id"
)

var argonParams = &argon2id.Params{
	Memory:      64 * 1024,
	Iterations:  3,
	Parallelism: 4,
	SaltLength:  16,
	KeyLength:   32,
}

func HashPassword(plain string) (string, error) {
	return argon2id.CreateHash(plain, argonParams)
}

func VerifyPassword(plain, hashed string) (bool, error) {
	return argon2id.ComparePasswordAndHash(plain, hashed)
}
