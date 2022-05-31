const firebase = require("firebase-admin");
const functions = require("firebase-functions");
const fs = firebase.firestore();

module.exports = async (req,res)=>{
    functions.logger.debug("deletePost");
    const postid = req.params.postid;
    const docRef = await fs.collection("foodlist").doc(postid).delete();
    return res.json({status:"OK"})
}