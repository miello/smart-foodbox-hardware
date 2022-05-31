const Busboy = require("busboy");
const firebase = require("firebase-admin");
const functions = require("firebase-functions");
const fs = require("fs");
const path = require("path");
const os = require("os");

const firestore = firebase.firestore();
const storage = firebase.storage();

module.exports = async (req, res) => {
  functions.logger.debug("weightData");
  const busboy = Busboy({ headers: req.headers });

  // This object will accumulate all the fields, keyed by their name
  const fields = {};

  // This code will process each non-file field in the form.
  // Credit: https://gist.github.com/msukmanowsky/c8daf3720c2839d3c535afc69234ab9e
  busboy.on("field", (fieldname, val) => {
    functions.logger.debug(`Found ${fieldname}: ${val}`);
    fields[fieldname] = val;
  });

  let fileWrites = {};

  // This code will process each file uploaded.
  busboy.on("file", (fieldname, file, { mimeType }) => {
    // Note: os.tmpdir() points to an in-memory file system on GCF
    // Thus, any files in it must fit in the instance's memory.
    functions.logger.debug(`Found ${fieldname}`);
    if (fieldname === "esp32-cam") {
      const newFileName = `${Date.now()}.jpg`;
      const filePath = path.join(os.tmpdir(), newFileName);
      fileWrites = { fileName: newFileName, filePath, mimeType };
      file.pipe(fs.createWriteStream(filePath));
    }
  });

  // Triggered once all uploaded files are processed by Busboy.
  // We still need to wait for the disk writes (saves) to complete.
  busboy.on("finish", async () => {
    await storage
      .bucket("smart-foodbox.appspot.com")
      .upload(fileWrites.filePath, {
        destination: fileWrites.fileName,
        metadata: {
          contentType: fileWrites.mimeType,
        },
      });

    const url = `https://firebasestorage.googleapis.com/v0/b/smart-foodbox.appspot.com/o/${fileWrites.fileName}?alt=media`;

    const docRef = await firestore.collection("foodlist").add({
      weight: fields["weight"],
      img: url,
      note: "",
      confirm: false,
      timeCreated: firebase.firestore.Timestamp.now(),
    });

    res.json({ status: "OK" });
  });

  busboy.end(req.rawBody);
};
