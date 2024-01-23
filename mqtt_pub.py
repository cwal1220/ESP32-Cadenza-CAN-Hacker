import paho.mqtt.client as mqtt

mqttc = mqtt.Client("python_pub") # puclisher 이름
mqttc.connect("138.2.126.137", 1883)


for i in range(5000):
    mqttc.publish("hehe", i) # topic, message