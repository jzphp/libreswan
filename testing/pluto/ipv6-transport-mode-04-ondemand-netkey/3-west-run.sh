# one ping will get lost in the ondemand as only TCP is cached
../../guestbin/ping-once.sh --forget -6 -I 2001:db8:1:2::45 2001:db8:1:2::23
../../guestbin/wait-for.sh --match v6-transport -- ipsec whack --trafficstatus
../../guestbin/ping-once.sh --up -6 -I 2001:db8:1:2::45 2001:db8:1:2::23
ipsec whack --trafficstatus
../../guestbin/ipsec-look.sh
echo done