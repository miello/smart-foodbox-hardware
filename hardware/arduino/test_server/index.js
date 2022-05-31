import express, { json, urlencoded, raw } from "express";
import multer from "multer";
import path from "path";
import fs from "fs";

const server = express();
server.use(json());
server.use(urlencoded({ extended: true }));
const storage = multer.diskStorage({
  destination: function (req, file, cb) {
    cb(null, "uploads/");
  },
  filename: function (req, file, cb) {
    cb(null, Date.now() + path.extname(file.originalname)); //Appending extension
  },
});

const upload = multer({ storage: storage });
const PORT = 8080;

try {
  fs.mkdirSync("uploads");
} catch (e) {}

server.post(
  "/test",
  upload.any(),
  // raw({ inflate: true, limit: "100kb", type: "multipart/form-data" }),
  (req, res) => {
    /**
     * This will return something like this
     *  [
     *     {
     *       fieldname: 'esp32-cam',
     *       originalname: 'Captured.JPG',
     *       encoding: '7bit',
     *       mimetype: 'image/jpeg',
     *       buffer: <Buffer ff d8 ff e0 00 10 4a 46 49 46 00 01 01 01 00 00 00 00 00 00 ff db 00 43 00 0a 07
     *   08 09 08 06 0a 09 08 09 0b 0b 0a 0c 0f 19 10 0f 0e 0e 0f 1f 16 17 12 ... 5527 more bytes>,
     *       size: 5577
     *     }
     *   ]
     */
    console.log(req.files);

    /**
     * Get current weight
     */
    console.log(+req.body.weight);
    res.status(200).send(`Success`);
  }
);

server.get("/", (req, res) => {
  res.send("Hello World");
});

server.listen(PORT, () => {
  console.log(`Server start at port ${PORT}`);
});
