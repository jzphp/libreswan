# say everything twice; but not using a retransmit
ipsec whack --impair suppress-retransmits
ipsec whack --impair jacob-two-two

ipsec auto --up westnet-eastnet-ipv4-psk-ikev2
../../guestbin/ping-once.sh --up -I 192.0.1.254 192.0.2.254
ipsec whack --trafficstatus

echo done