package auth

// AccessTTLSec is a small helper so handlers can emit the "expires_in"
// field in login responses without reaching into the signer internals.
func (s *Signer) AccessTTLSec() int64 { return int64(s.accessTTL.Seconds()) }
