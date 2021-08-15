/*
 * Aventuro - A text aventure system in Esperanto
 * Copyright (C) 2021  Neil Roberts
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

(function ()
 {
   var hasRun = false;
   var avtData = null;
   var hasRuntime = false;
   var seekPos = 0;
   var avt;
   var avtState = 0;
   var inputbox;
   var messagesDiv;
   var statusMessageDiv;
   var restartButton;
   var gameIsOver = false;

   function seekAvtData(source, pos, error)
   {
     seekPos = pos;
     return 1;
   }

   function readAvtData(source, memory, length)
   {
     if (seekPos + length > avtData.byteLength)
       length = avtData.byteLength - seekPos;

     if (length <= 0)
       return 0;

     var slice = new Uint8Array(avtData.slice(seekPos, seekPos + length));
     writeArrayToMemory(slice, memory);

     seekPos += length;

     return length;
   }

   function addMessage(klass, text)
   {
     var messageDiv = document.createElement("div");
     messageDiv.className = "message " + klass;
     messagesDiv.appendChild(messageDiv);

     var innerDiv = document.createElement("div");
     innerDiv.className = "messageInner";
     messageDiv.appendChild(innerDiv);

     var textDiv = document.createElement("div");
     textDiv.className = "messageText";
     innerDiv.appendChild(textDiv);

     var lastPos = 0;
     var nextPos;

     while ((nextPos = text.indexOf("\n", lastPos)) != -1) {
       if (nextPos > lastPos) {
         var sub = text.substring(lastPos, nextPos);
         var textNode = document.createTextNode(sub);
         textDiv.appendChild(textNode);
       }

       textDiv.appendChild(document.createElement("br"));

       lastPos = nextPos + 1;
     }

     if (lastPos < text.length) {
       var textNode = document.createTextNode(text.substring(lastPos));
       textDiv.appendChild(textNode);
     }

     return textDiv;
   }

   function showAtTime(div, delay)
   {
     div.style.opacity = 0.0;
     function callback() {
       div.style.opacity = 1.0;
       div.className = "fadeInText";
     };
     setTimeout(callback, delay * 1000);
   }

   function processMessages()
   {
     if (!gameIsOver && _pcx_avt_state_game_is_over(avtState)) {
       gameIsOver = true;
       inputbox.contentEditable = false;
       restartButton.style.display = "block";
     }

     var lastDiv = null;
     var messageDelay = 0;

     while (true) {
       var msg = _pcx_avt_state_get_next_message(avtState);

       if (msg == 0)
         break;

       var msgType = getValue(msg, 'i32');
       var text = UTF8ToString(msg + 4);

       if (msgType == 0 || lastDiv == null) {
         lastDiv = addMessage("note", text);
         if (messageDelay > 0)
           showAtTime(lastDiv, messageDelay);
       } else {
         var textNode = document.createTextNode(" " + text);
         var span = document.createElement("span");
         span.appendChild(textNode);
         lastDiv.appendChild(span);
         showAtTime(span, ++messageDelay);
       }
     }

     messagesDiv.scrollTo(0,
                          messagesDiv.scrollHeight -
                          messagesDiv.clientHeight);
   }

   function setRoomName()
   {
     var roomNameStr = _pcx_avt_state_get_current_room_name(avtState);
     var roomName = UTF8ToString(roomNameStr);
     var roomNameNode = document.createTextNode(roomName);

     statusMessageDiv.innerHTML = "";
     statusMessageDiv.appendChild(roomNameNode);
   }

   function sendCommand()
   {
     if (gameIsOver)
       return;

     var text = inputbox.innerText.replace(/[\x01- ]/g, ' ');

     if (text.match(/\S/) == null)
       return;

     inputbox.innerHTML = "";

     addMessage("command", text);

     var lengthBytes = lengthBytesUTF8(text) + 1;
     var stringOnWasmHeap = _malloc(lengthBytes);
     stringToUTF8(text, stringOnWasmHeap, lengthBytes);
     _pcx_avt_state_run_command(avtState, stringOnWasmHeap);
     _free(stringOnWasmHeap);

     processMessages();
     setRoomName();
   }

   function commandKeyCb(e)
   {
     if (e.which != 10 && e.which != 13)
       return;

     event.preventDefault();

     sendCommand();
   }

   function addTitleMessage()
   {
     var namePtr = getValue(avt + 0, '*');
     var name = UTF8ToString(namePtr);
     var authorPtr = getValue(avt + 4, '*');
     var author = UTF8ToString(authorPtr);
     var yearPtr = getValue(avt + 8, '*');
     var year = UTF8ToString(yearPtr);

     var textDiv = addMessage("gameTitle", "© " + author + " " + year);
     var br = document.createElement("br");
     textDiv.insertBefore(br, textDiv.firstChild);

     var nameNode = document.createElement("b");
     nameNode.appendChild(document.createTextNode(name));
     textDiv.insertBefore(nameNode, br);

     document.title = name;
     var chatTitle = document.getElementById("chatTitleText");
     chatTitle.innerHTML = "";
     chatTitle.appendChild(document.createTextNode(name));
   }

   function startGame()
   {
     if (!hasRun)
       return;
     if (avtState != 0)
       _pcx_avt_state_free(avtState);

     messagesDiv.innerHTML = "";
     restartButton.style.display = "none";
     inputbox.contentEditable = true;
     gameIsOver = false;

     avtState = _pcx_avt_state_new(avt);

     addTitleMessage();

     processMessages();
     setRoomName();
   }

   function checkRun()
   {
     if (hasRun || avtData == null || !hasRuntime)
       return;

     hasRun = true;

     var source = _malloc(8);
     var seek = addFunction(seekAvtData, 'iiii');
     var read = addFunction(readAvtData, 'iiii');
     setValue(source, seek, '*');
     setValue(source + 4, read, '*');

     var errPtr = _malloc(4);
     setValue(errPtr, 0, '*');
     avt = _pcx_load_or_parse(source, errPtr);
     var err = getValue(errPtr, '*');
     _free(errPtr);

     avtData = null;

     if (avt == 0) {
       _pcx_error_free(err);
       console.log("Eraro dum la ŝargo de la AVT-dosiero");
       return;
     }

     inputbox = document.getElementById("inputbox");
     inputbox.addEventListener("keydown", commandKeyCb);
     messagesDiv = document.getElementById("messages");
     statusMessageDiv = document.getElementById("statusMessage");
     statusMessageDiv.style.display = "block";
     restartButton = document.getElementById("restartButton");
     restartButton.onclick = startGame;

     document.getElementById("sendButton").onclick = sendCommand;

     startGame();
   }

   function gotRuntime()
   {
     hasRuntime = true;
     checkRun();
   }

   var ajax = new XMLHttpRequest();

   function gameLoaded()
   {
     avtData = ajax.response;
     checkRun();
   }

   ajax.responseType = "arraybuffer";
   ajax.addEventListener("load", gameLoaded);
   ajax.open("GET", "ludo.avt");
   ajax.send(null);

   Module.onRuntimeInitialized = gotRuntime;

 })();
