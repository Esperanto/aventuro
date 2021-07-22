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
   var avtState;
   var inputBox;
   var messagesDiv;

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
     var isScrolledToBottom = (messagesDiv.scrollHeight -
                               messagesDiv.scrollTop <=
                               messagesDiv.clientHeight * 1.75);

     var messageDiv = document.createElement("div");
     messageDiv.className = "message " + klass;

     var innerDiv = document.createElement("div");
     innerDiv.className = "messageInner";

     var textDiv = document.createElement("div");
     textDiv.className = "messageText";

     var textNode = document.createTextNode(text);
     innerDiv.appendChild(textNode);

     messageDiv.appendChild(innerDiv);

     messagesDiv.appendChild(messageDiv);

     if (isScrolledToBottom) {
       messagesDiv.scrollTo(0,
                            messagesDiv.scrollHeight -
                            messagesDiv.clientHeight);
     }
   }

   function processMessages()
   {
     while (true) {
       var msg = _pcx_avt_state_get_next_message(avtState);

       if (msg == 0)
         break;

       addMessage("note", UTF8ToString(msg + 4));
     }
   }

   function sendCommand()
   {
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
   }

   function commandKeyCb(e)
   {
     if (e.which != 10 && e.which != 13)
       return;

     event.preventDefault();

     sendCommand();
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

     var err = _malloc(4);
     avt = _pcx_avt_load(source, err);

     avtData = null;

     if (avt == 0) {
       console.log("Eraro dum la ŝargo de la AVT-dosiero");
       return;
     }

     avtState = _pcx_avt_state_new(avt);

     inputbox = document.getElementById("inputbox");
     inputbox.addEventListener("keydown", commandKeyCb);
     messagesDiv = document.getElementById("messages");

     document.getElementById("sendButton").onclick = sendCommand;

     processMessages();
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
