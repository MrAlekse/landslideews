// Firebase Configuration
const firebaseConfig = {
    apiKey: "AIzaSyDeTOMvduzpL7ckLKzPInFtODKm2KUYioE",
    authDomain: "lora-landslide-warning.firebaseapp.com",
    projectId: "lora-landslide-warning",
    storageBucket: "lora-landslide-warning.firebasestorage.app",
    messagingSenderId: "1097663983219",
    appId: "1:1097663983219:web:b031dcaae1f910264e4406"
};

firebase.initializeApp(firebaseConfig);
const db = firebase.firestore();

let chart;
let sensorData = { labels: [], soilData: [], rainData: [] };

// ===== MENU FUNCTIONALITY =====
document.addEventListener('DOMContentLoaded', function() {
    const menuButton = document.getElementById('menuButton');
    const closeMenu = document.getElementById('closeMenu');
    const sideMenu = document.getElementById('sideMenu');
    const menuOverlay = document.getElementById('menuOverlay');
    const menuTabs = document.querySelectorAll('.menu-tab');
    const tabContents = document.querySelectorAll('.tab-content');

    // Open menu
    menuButton.addEventListener('click', () => {
        sideMenu.classList.add('active');
        menuOverlay.classList.add('active');
    });

    // Close menu
    closeMenu.addEventListener('click', closeMenuFunc);
    menuOverlay.addEventListener('click', closeMenuFunc);

    function closeMenuFunc() {
        sideMenu.classList.remove('active');
        menuOverlay.classList.remove('active');
    }

    // Tab switching
    menuTabs.forEach(tab => {
        tab.addEventListener('click', () => {
            const tabName = tab.dataset.tab;
            
            // Remove active class from all
            menuTabs.forEach(t => t.classList.remove('active'));
            tabContents.forEach(c => c.classList.remove('active'));
            
            // Add active class to clicked
            tab.classList.add('active');
            document.getElementById(tabName + '-content').classList.add('active');
        });
    });

    // Historical data loader
    document.getElementById('loadHistoricalData').addEventListener('click', loadHistoricalData);

    // Initialize everything
    updateDateTime();
    setInterval(updateDateTime, 1000);
    initChart();
    listenToSensorData();
    
    document.getElementById('system-status').textContent = 'Live';
    document.getElementById('system-status').style.color = '#27ae60';
});

// ===== DATE/TIME =====
function updateDateTime() {
    const now = new Date();
    const dateOptions = { year: 'numeric', month: 'long', day: 'numeric' };
    const timeOptions = { hour: 'numeric', minute: '2-digit', second: '2-digit', hour12: true };
    
    document.getElementById('current-date').textContent = now.toLocaleDateString('en-US', dateOptions);
    document.getElementById('current-time').textContent = now.toLocaleTimeString('en-US', timeOptions);
}

// ===== CHART =====
function initChart() {
    const ctx = document.getElementById('sensorChart').getContext('2d');
    chart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: sensorData.labels,
            datasets: [
                {
                    label: 'Soil Moisture (%)',
                    data: sensorData.soilData,
                    borderColor: '#2563eb',
                    backgroundColor: 'rgba(37, 99, 235, 0.2)',
                    tension: 0.4,
                    borderWidth: 3,
                    pointRadius: 5,
                    pointBackgroundColor: '#2563eb',
                    pointBorderColor: '#000',
                    pointBorderWidth: 2
                },
                {
                    label: 'Rain Intensity',
                    data: sensorData.rainData,
                    borderColor: '#7c3aed',
                    backgroundColor: 'rgba(124, 58, 237, 0.2)',
                    tension: 0.4,
                    borderWidth: 3,
                    pointRadius: 5,
                    pointBackgroundColor: '#7c3aed',
                    pointBorderColor: '#000',
                    pointBorderWidth: 2
                }
            ]
        },
        options: {
            responsive: true,
            maintainAspectRatio: true,
            aspectRatio: 2.5,
            plugins: {
                legend: {
                    display: true,
                    position: 'top',
                    labels: {
                        color: '#000000',
                        font: { size: 14, weight: 'bold' },
                        padding: 15,
                        usePointStyle: true
                    }
                }
            },
            scales: {
                x: {
                    title: {
                        display: true,
                        text: 'Time (Hours)',
                        color: '#000000',
                        font: { size: 14, weight: 'bold' }
                    },
                    ticks: { color: '#000000', font: { size: 12, weight: '600' } },
                    grid: { color: 'rgba(0, 0, 0, 0.3)', borderColor: '#000000' }
                },
                y: {
                    title: {
                        display: true,
                        text: 'Value',
                        color: '#000000',
                        font: { size: 14, weight: 'bold' }
                    },
                    ticks: { color: '#000000', font: { size: 12, weight: '600' } },
                    grid: { color: 'rgba(0, 0, 0, 0.3)', borderColor: '#000000' },
                    beginAtZero: true
                }
            }
        }
    });
}

// ===== SENSOR DATA =====
function updateSensorValues(data) {
    document.getElementById('soil-value').textContent = 
        data.soilMoisture ? data.soilMoisture.toFixed(1) + ' %' : '-- %';
    
    document.getElementById('rain-value').textContent = 
        data.rainIntensity ? data.rainIntensity.toFixed(1) + ' mm' : '-- mm';
    
    document.getElementById('tilt-value').textContent = 
        data.tilt ? Math.sqrt(data.tilt.x**2 + data.tilt.y**2 + data.tilt.z**2).toFixed(1) + '°' : '--°';
    
    updateAlertLevel(data.alertLevel || 0);
    
    const now = new Date();
    const timeLabel = now.getHours() + ':' + String(now.getMinutes()).padStart(2, '0');
    
    sensorData.labels.push(timeLabel);
    sensorData.soilData.push(data.soilMoisture || 0);
    sensorData.rainData.push(data.rainIntensity || 0);
    
    if (sensorData.labels.length > 20) {
        sensorData.labels.shift();
        sensorData.soilData.shift();
        sensorData.rainData.shift();
    }
    
    if (chart) chart.update();
}

function updateAlertLevel(level) {
    const alertBanner = document.getElementById('alert-banner');
    const alertText = document.getElementById('alert-text');
    
    // Updated to match thesis documentation
    const levels = {
        0: { text: 'Level 0 (Normal - Low Risk)', color: '#27ae60' },
        1: { text: 'Level 1 (Caution - Moderate Risk)', color: '#f1c40f' },
        2: { text: 'Level 2 (Warning - High Risk)', color: '#e67e22' },
        3: { text: 'Level 3 (Danger - Very High Risk)', color: '#e74c3c' }
    };
    
    const config = levels[level] || levels[0];
    alertText.textContent = config.text;
    alertBanner.style.background = config.color;
}

function listenToSensorData() {
    db.collection('status').doc('latest').onSnapshot((doc) => {
        if (doc.exists) {
            console.log('New sensor data:', doc.data());
            updateSensorValues(doc.data());
        }
    }, (error) => {
        console.error('Error:', error);
        document.getElementById('system-status').textContent = 'Offline';
        document.getElementById('system-status').style.color = '#e74c3c';
    });
}

// ===== HISTORICAL DATA =====
async function loadHistoricalData() {
    const datePicker = document.getElementById('datePicker');
    const selectedDate = datePicker.value;
    const summaryDiv = document.getElementById('historicalSummary');
    
    if (!selectedDate) {
        summaryDiv.innerHTML = '<p class="no-data">Please select a date</p>';
        return;
    }
    
    summaryDiv.innerHTML = '<p style="text-align:center;"><i class="fas fa-spinner fa-spin"></i> Loading...</p>';
    
    try {
        const startDate = new Date(selectedDate);
        startDate.setHours(0, 0, 0, 0);
        const endDate = new Date(selectedDate);
        endDate.setHours(23, 59, 59, 999);
        
        const snapshot = await db.collection('sensor_readings')
            .where('timestamp', '>=', startDate)
            .where('timestamp', '<=', endDate)
            .orderBy('timestamp', 'desc')
            .limit(50)
            .get();
        
        if (snapshot.empty) {
            summaryDiv.innerHTML = '<p class="no-data">No data found for this date</p>';
            return;
        }
        
        let html = '<div style="max-height: 400px; overflow-y: auto;">';
        snapshot.forEach(doc => {
            const data = doc.data();
            const time = data.timestamp ? data.timestamp.toDate().toLocaleTimeString() : '--';
            
            // Updated alert level names
            const alertNames = ['Normal', 'Caution', 'Warning', 'Danger'];
            const alertName = alertNames[data.alertLevel] || 'Unknown';
            
            html += `
                <div style="background: rgba(255,255,255,0.1); padding: 1rem; margin-bottom: 0.5rem; border-radius: 8px;">
                    <strong>${time}</strong><br>
                    Soil: ${data.soilMoisture}% | Rain: ${data.rainIntensity}mm | Alert: Level ${data.alertLevel} (${alertName})
                </div>
            `;
        });
        html += '</div>';
        summaryDiv.innerHTML = html;
        
    } catch (error) {
        console.error('Error loading historical data:', error);
        summaryDiv.innerHTML = '<p class="no-data" style="color: #e74c3c;">Error loading data</p>';
    }
}
