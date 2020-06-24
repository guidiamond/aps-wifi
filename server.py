from flask import Flask, render_template, request, jsonify
from flask_restful import Resource, Api

app = Flask(__name__)

# global led
# led = 0


@app.route('/status', methods=['POST', 'GET'])
def status():
    if request.method == 'POST':
        req = request.get_json()
        timestamp = req.get('timestamp')
        id = req.get('id')
        print("O request vale : ", timestamp, id)
        print(req)
        return jsonify({'sucess': 1}), 200
    elif request.method == 'GET':
        return jsonify({'server_status': 1})
    else:
        return jsonify({'sucess': 0}), 200

@app.route('/', methods=['GET'])
def index():
    if request.method == 'GET':
        return jsonify({'server_status': 1})

if __name__ == '__main__':
    app.run(host='0.0.0.0', debug=False)
