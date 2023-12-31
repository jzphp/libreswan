# first ping will be lost in triggering OE
../../guestbin/ping-once.sh --down -I 192.1.3.209 192.1.2.23
../../guestbin/wait-for.sh --match private-or-clear -- ipsec whack --trafficstatus
# issue a manual rekey of parent
ipsec whack --rekey-ike --name '#1'
# wait two seconds to ensure old parent has expired
sleep 2
# parent state must be #3 and the latest ISAKMP
ipsec showstates
ipsec whack --trafficstatus
ipsec whack --shuntstatus
echo done
