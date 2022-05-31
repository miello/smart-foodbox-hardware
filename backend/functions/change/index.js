const express = require("express")
const router = express.Router()

const weightData = require("./weightData")
const addNote = require("./addNote")
const deletePost = require("./deletePost")


router.post("/weightchange", weightData);
router.delete("/post/:postid", deletePost);
router.post("/post/:postid", addNote);

const app = express()
const cors = require("cors")
app.use(cors({origin: true}))
app.use(router)
module.exports = app