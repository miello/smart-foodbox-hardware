const firebase = require("firebase-admin");
const functions = require("firebase-functions");
const fs = firebase.firestore();

module.exports = async (req,res)=>{
    functions.logger.debug("addNote");
    const postid = req.params.postid;
    const note = req.body.note;
    const docRef = await fs.collection("foodlist").doc(postid).set({
        note,
        confirm: true,
    },{ merge: true });
    return res.json({status:"OK"})
}