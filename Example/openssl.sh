#!/bin/bash
#echo "Start openssl pid:" $$
#sleep 2
echo "Q" | openssl s_client -connect google.com:443 -tls1_2
#echo "Q" | openssl s_client -connect localhost:8080 -tls1_2
#echo "-----------------------------------------------------------------------"
