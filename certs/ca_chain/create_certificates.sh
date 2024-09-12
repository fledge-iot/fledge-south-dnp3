## Generate fledgeDNP3ca.key and fledgeDNP3ca.cert


openssl req -x509 -new -nodes -newkey rsa:2048 -keyout fledgeDNP3ca.key -sha256 -days 3600 -out fledgeDNP3ca.cert -subj "/C=US/ST=OR/L=Bend/O=Acme Certificate Corp"

## Generate mastr and outstation cerificates, every certificate is part of CA chain fledgeDNP3ca
openssl req -newkey rsa:2048 -nodes -keyout master1.key -out master1.csr -subj "/C=US/ST=OR/O=FledgeDNP3"
openssl req -newkey rsa:2048 -nodes -keyout outstation1.key -out outstation1.csr -subj "/C=US/ST=OR/O=FledgeDNP3"
openssl req -newkey rsa:2048 -nodes -keyout outstation2.key -out outstation2.csr -subj "/C=US/ST=OR/O=FledgeDNP3"

openssl x509 -req -in master1.csr -CA fledgeDNP3ca.cert -CAkey fledgeDNP3ca.key -CAcreateserial -out master1.cert -days 3600 -sha256
openssl x509 -req -in outstation1.csr -CA fledgeDNP3ca.cert -CAkey fledgeDNP3ca.key -CAcreateserial -out outstation1.cert -days 3600 -sha256
openssl x509 -req -in outstation2.csr -CA fledgeDNP3ca.cert -CAkey fledgeDNP3ca.key -CAcreateserial -out outstation2.cert -days 3600 -sha256

### 1. Add fledgeDNP3ca.cert, master1.key, master1.cert into Fledge store and set fledgeDNP3ca and master1 as certificate names and enable TLS
### 2. Add fledgeDNP3ca.cert,outstation1.key, outstation1.cert into DNP3 outstation server and use them with TLS enabled

### Verify CA CHAIN
openssl verify -CAfile fledgeDNP3ca.cert master1.cert outstation1.cert outstation2.cert

