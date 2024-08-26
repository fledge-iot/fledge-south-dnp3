openssl req -x509 -new -nodes -newkey rsa:2048 -keyout fledgeDNP3ca_key.pem -sha256 -days 3600 -out fledgeDNP3ca_cert.pem -subj "/C=US/ST=OR/L=Bend/O=Acme Certificate Corp"

openssl req -newkey rsa:2048 -nodes -keyout master1_key.pem -out master1.csr -subj "/C=US/ST=OR/O=FledgeDNP3"
openssl req -newkey rsa:2048 -nodes -keyout outstation1_key.pem -out outstation1.csr -subj "/C=US/ST=OR/O=FledgeDNP3"
openssl req -newkey rsa:2048 -nodes -keyout outstation2_key.pem -out outstation2.csr -subj "/C=US/ST=OR/O=FledgeDNP3"

openssl x509 -req -in master1.csr -CA fledgeDNP3ca_cert.pem -CAkey fledgeDNP3ca_key.pem -CAcreateserial -out master1_cert.pem -days 3600 -sha256
openssl x509 -req -in outstation1.csr -CA fledgeDNP3ca_cert.pem -CAkey fledgeDNP3ca_key.pem -CAcreateserial -out outstation1_cert.pem -days 3600 -sha256
openssl x509 -req -in outstation2.csr -CA fledgeDNP3ca_cert.pem -CAkey fledgeDNP3ca_key.pem -CAcreateserial -out outstation2_cert.pem -days 3600 -sha256

openssl verify -CAfile fledgeDNP3ca_cert.pem master1_cert.pem outstation1_cert.pem outstation2_cert.pem

