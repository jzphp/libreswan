# don't come back
ipsec whack --impair revival
# expect quick fail
ipsec whack --impair ke-payload:omit
ipsec whack --impair timeout-on-retransmit
ipsec auto --up westnet-eastnet-ipv4-psk-ikev2
ipsec whack --impair none
# expect slower fail
ipsec whack --impair suppress-retransmits
ipsec auto --up westnet-eastnet-ipv4-psk-ikev2
echo done
