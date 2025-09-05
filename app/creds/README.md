# Management of credentials

## Create a device certificate

First install [step](https://smallstep.com/docs/step-cli/installation/).

```
apt-get update && apt-get install -y --no-install-recommends curl vim gpg ca-certificates
curl -fsSL https://packages.smallstep.com/keys/apt/repo-signing-key.gpg -o /etc/apt/trusted.gpg.d/smallstep.asc && \
    echo 'deb [signed-by=/etc/apt/trusted.gpg.d/smallstep.asc] https://packages.smallstep.com/stable/debian debs main' \
    | tee /etc/apt/sources.list.d/smallstep.list
apt-get update && apt-get -y install step-cli
```

Then generate new device certificate:

```
step certificate create wind_turbine_stm32f746g_disco wind_turbine_stm32f746g_disco-certificate.pem.crt wind_turbine_stm32f746g_disco-private.pem.key --profile self-signed --subtle --no-password --insecure --not-before -24h --not-after 87600h
```

## Provisionning Kamea platform

Import the certificate previously generated in Kamea MQTT configuration.

Retrieve MQTT broker CA at the same time.


## Testing communication with mosquitto_pub

Execute the following command to test the communication.

If using the production Kamea environment:

```
mosquitto_pub -h kprod-rmq.westeurope.cloudapp.azure.com -m "{ \"msg\": \"test message\" }" -t "device/wind_turbine_stm32f746g_disco/telemetries" --cafile broker_ca.crt --cert wind_turbine_stm32f746g_disco-certificate.pem.crt --key wind_turbine_stm32f746g_disco-private.pem.key -p 8883 -d
```

If using the development Kamea environment:

```
mosquitto_pub -h kdev-rmq.westeurope.cloudapp.azure.com -m "{ \"msg\": \"test message\" }" -t "device/wind_turbine_stm32f746g_disco/telemetries" --cafile broker_ca.crt --cert wind_turbine_stm32f746g_disco-certificate.pem.crt --key wind_turbine_stm32f746g_disco-private.pem.key -p 8883 -d
```

## Convert credentials to build MCU firmware

Execute the following command to convert the credentials.

```
python3 convert_keys.py
```

This is generating: `ca.c`, `cert.c` and `key.c`.
