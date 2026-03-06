const express = require('express');
const admin = require('firebase-admin');
const cors = require('cors');
const bodyParser = require('body-parser');
const path = require('path');

const app = express();

app.use(cors());
app.use(bodyParser.json());

// Serve frontend files
app.use(express.static(path.join(__dirname, "../frontend")));

app.get("/", (req, res) => {
  res.sendFile(path.join(__dirname, "../frontend/index.html"));
});

// Firebase
const serviceAccount = require('./serviceAccountKey.json');

admin.initializeApp({
  credential: admin.credential.cert(serviceAccount)
});

const db = admin.firestore();

// API endpoint
app.post('/api/data', async (req, res) => {
  try {
    const { data } = req.body;
    if (!data) return res.status(400).send('No data provided');

    const parts = data.split(',');

    const sensorData = {
      soilMoisture: parseFloat(parts[0]),
      rainIntensity: parseFloat(parts[1]),
      tilt: {
        x: parseFloat(parts[2]),
        y: parseFloat(parts[3]),
        z: parseFloat(parts[4])
      },
      alertLevel: parseInt(parts[5]),
      timestamp: admin.firestore.FieldValue.serverTimestamp()
    };

    await db.collection('sensor_readings').add(sensorData);
    await db.collection('status').doc('latest').set(sensorData);

    console.log('Data saved:', sensorData);
    res.status(200).json({ message: 'Data received and stored' });

  } catch (error) {
    console.error('Error saving data:', error);
    res.status(500).send('Internal Server Error');
  }
});

const PORT = process.env.PORT || 3000;
app.listen(PORT, () => {
  console.log(`Server running at http://127.0.0.1:${PORT}`);
});
