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
   var avtDataLength = 0;
   var hasRuntime = false;
   var seekPos = 0;
   var avt = 0;
   var avtState = 0;
   var inputbox;
   var messagesDiv;
   var statusMessageDiv;
   var restartButton;
   var gameIsOver = false;
   var editorText;

   function bufferLength(buf)
   {
     return getValue(buf + 4, 'i32');
   }

   function allocateBuffer()
   {
     var buf = _malloc(16);
     _pcx_buffer_init(buf);
     return buf;
   }

   function addStringToBuffer(buf, str)
   {
     var oldLength = bufferLength(buf);
     var strLength = lengthBytesUTF8(str);
     _pcx_buffer_ensure_size(buf, oldLength + strLength + 1);
     var data = getValue(buf, '*') + oldLength;
     stringToUTF8(str, data, strLength + 1);
     _pcx_buffer_set_length(buf, oldLength + strLength);
   }

   function freeBuffer(buf)
   {
     _pcx_buffer_destroy(buf);
     _free(buf);
   }

   function shouldAddNewline(buf, elem)
   {
     var name = elem.localName.toUpperCase();

     if (name == 'BR')
       return true;

     if (name != 'DIV' && name != 'P')
       return false;

     var length = bufferLength(buf);
     var data = getValue(buf, '*');

     return length > 0 && getValue(data + length - 1, 'i8') != 10;
   }

   function findNodeForLine(root, targetLine)
   {
     var stack = [root.firstChild];
     var lineNum = 1;
     var atLineStart = true;

     while (stack.length > 0) {
       var node = stack.pop();

       if (node == null)
         continue;

       stack.push(node.nextSibling);

       if (node instanceof Element) {
         var name = node.localName.toUpperCase();
         if (name == 'BR') {
           lineNum++;
           atLineStart = true;
         } else if (name == 'DIV' || name == 'P') {
           if (!atLineStart) {
             lineNum++;
             atLineStart = true;
           }
         }
         stack.push(node.firstChild);
       } else if (node instanceof Text) {
         if (lineNum >= targetLine)
           return node;
         if (node.data.length > 0)
           atLineStart = false;
       }
     }

     return null;
   }

   function jumpToEditorLine(lineNum)
   {
     var node = findNodeForLine(editorText, lineNum);

     if (node == null) {
       node = editorText.lastChild;
       if (node == null)
         node = editorText;
     }

     var range = document.createRange();
     range.setStart(node, 0);
     range.collapse(true);
     var selection = window.getSelection();
     selection.removeAllRanges();
     selection.addRange(range);

     node.parentNode.scrollIntoView();
   }

   function addDomToBuffer(buf, node)
   {
     if (node instanceof Element) {
       if (shouldAddNewline(buf, node))
         addStringToBuffer(buf, "\n");
       var child;
       for (child = node.firstChild; child != null; child = child.nextSibling)
         addDomToBuffer(buf, child);
     } else if (node instanceof Text) {
       addStringToBuffer(buf, node.data);
     }
   }

   function seekAvtData(source, pos, error)
   {
     seekPos = pos;
     return 1;
   }

   function readAvtData(source, memory, length)
   {
     if (seekPos + length > avtDataLength)
       length = avtDataLength - seekPos;

     if (length <= 0)
       return 0;

     if (typeof avtData === 'number') {
       HEAPU8.copyWithin(memory, avtData + seekPos, avtData + seekPos + length);
     } else {
       var slice = new Uint8Array(avtData.slice(seekPos, seekPos + length));
       writeArrayToMemory(slice, memory);
     }

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
     if (gameIsOver || avt == 0 || avtState == 0)
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

     if (editorText == null)
       document.title = name;

     var chatTitle = document.getElementById("chatTitleText");
     chatTitle.innerHTML = "";
     chatTitle.appendChild(document.createTextNode(name));
   }

   function startGame()
   {
     if (!hasRun || avt == 0)
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

   function loadAvtData()
   {
     seekPos = 0;

     var source = _malloc(8);
     var seek = addFunction(seekAvtData, 'iiii');
     var read = addFunction(readAvtData, 'iiii');
     setValue(source, seek, '*');
     setValue(source + 4, read, '*');

     var errPtr = _malloc(4);
     setValue(errPtr, 0, '*');
     var newAvt = _pcx_load_or_parse(source, errPtr);
     var err = getValue(errPtr, '*');

     _free(errPtr);
     _free(source);

     if (newAvt == 0) {
       var errMsg = UTF8ToString(err + 8);
       _pcx_error_free(err);
       console.log("Eraro dum la ŝargo de la AVT-dosiero: " + errMsg);
       return errMsg;
     }

     if (avtState != 0) {
       _pcx_avt_state_free(avtState);
       avtState = 0;
     }
     if (avt != 0)
       _pcx_avt_free(avt);

     avt = newAvt;

     startGame();

     return null;
   }

   function checkRun()
   {
     if (hasRun || avtData == null || !hasRuntime)
       return;

     hasRun = true;

     loadAvtData();
     avtData = null;
   }

   function gotRuntime()
   {
     hasRuntime = true;
     checkRun();
   }

   function clearEditorMessage()
   {
     document.getElementById("editorMessage").style.display = "none";
   }

   function editorRun()
   {
     var buf = allocateBuffer();
     addDomToBuffer(buf, editorText);
     avtDataLength = bufferLength(buf);
     avtData = getValue(buf, '*');

     var errMsg = loadAvtData();

     freeBuffer(buf);

     if (errMsg != null) {
       var messageNode = document.createTextNode(errMsg);

       var match = errMsg.match(/^linio ([0-9]+):/);
       if (match) {
         var lineNum = parseInt(match[1]);
         jumpToEditorLine(lineNum);
         var a = document.createElement("a");
         a.href = "#";
         a.onclick = function() { jumpToEditorLine(lineNum); };
         a.appendChild(messageNode);
         messageNode = a;
       }

       var textSpan = document.getElementById("editorMessageText");
       textSpan.innerHTML = "";
       textSpan.appendChild(messageNode);
       document.getElementById("editorMessage").style.display = "block";
     } else {
       clearEditorMessage();
     }
   }

   var ajax = new XMLHttpRequest();

   function gameLoaded()
   {
     avtData = ajax.response;
     avtDataLength = avtData.byteLength;
     checkRun();
   }

   inputbox = document.getElementById("inputbox");
   inputbox.addEventListener("keydown", commandKeyCb);
   messagesDiv = document.getElementById("messages");
   statusMessageDiv = document.getElementById("statusMessage");
   statusMessageDiv.style.display = "block";
   restartButton = document.getElementById("restartButton");
   restartButton.onclick = startGame;
   editorText = document.getElementById("editorText");

   document.getElementById("sendButton").onclick = sendCommand;

   if (editorText != null) {
     document.getElementById("runButton").onclick = editorRun;
     document.getElementById("closeEditorMessage").onclick = clearEditorMessage;
   }

   ajax.responseType = "arraybuffer";
   ajax.addEventListener("load", gameLoaded);
   ajax.open("GET", "ludo.avt");
   ajax.send(null);

   Module.onRuntimeInitialized = gotRuntime;

 })();
