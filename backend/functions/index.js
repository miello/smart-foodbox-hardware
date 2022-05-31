const functions = require("firebase-functions");
const firebase = require("firebase-admin");
firebase.initializeApp({
    // databaseURL: functions.config().project.databaseurl,
    storageBucket: "smart-foodbox.appspot.com",
    apiKey: "AIzaSyAvSim3QEiZM_edEugQNnVuIN_uJthLIYc",
})

const change = require("./change");
exports.api = functions.region("asia-east2").https.onRequest(change);