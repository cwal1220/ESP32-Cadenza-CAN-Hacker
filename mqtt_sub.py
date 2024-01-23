import paho.mqtt.client as mqtt

# subscriber callback
def on_message(client, userdata, message):
        print("message received ", str(message.payload.decode("utf-8")))
        print("message topic= ", message.topic)
        print("message qos=", message.qos)
        print("message retain flag= ", message.retain)

broker_address = "138.2.126.137"
client1 = mqtt.Client("client1")
client1.connect(broker_address)
client1.subscribe("WEIGHT_230507/LEFT")
client1.subscribe("WEIGHT_230507/RIGHT")
client1.on_message = on_message
client1.loop_forever()