from flask import Flask, render_template, request, jsonify
from flask_restful import Resource, Api

app = Flask(__name__)

# global led
# led = 0


@app.route('/')
def control():
    return render_template('index.html')


@app.route('/status', methods=['POST', 'GET'])
def status():
    led = 0
    # global led
    if request.method == 'POST':
        req = request.get_json()
        timestamp = req.get('timestamp')
        id = req.get('id')
        print("O request vale : ", timestamp, id)
        print(req)
        return jsonify({'success': 1})
       # status = request.form
       # led = status['LED']
       # return render_template("status.html", status = status)
    else:
        return jsonify({'led': led}), 200


if __name__ == '__main__':
    app.run(host='0.0.0.0', debug=False)
