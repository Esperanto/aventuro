(function ()
 {
   var hasRun = false;
   var avtData = null;
   var hasRuntime = false;
   var seekPos = 0;
   var avt;
   var avtState;
   var inputBox;

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
     var elem = document.createTextNode(text);
     var div = document.createElement("div");
     div.className = klass;
     div.appendChild(elem);
     document.getElementById("messages").appendChild(div);
   }

   function processMessages()
   {
     while (true) {
       var msg = _pcx_avt_state_get_next_message(avtState);

       if (msg == 0)
         break;

       addMessage("message", UTF8ToString(msg));
     }
   }

   function commandKeyCb(e)
   {
     if (e.which != 10 && e.which != 13)
       return;

     var value = inputBox.value;

     if (!value || value.length <= 0)
       return;

     inputBox.value = "";

     addMessage("command", value);

     var lengthBytes = lengthBytesUTF8(value) + 1;
     var stringOnWasmHeap = _malloc(lengthBytes);
     stringToUTF8(value, stringOnWasmHeap, lengthBytes);
     _pcx_avt_state_run_command(avtState, stringOnWasmHeap);
     _free(stringOnWasmHeap);

     processMessages();
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

     processMessages();

     inputBox = document.getElementById("command-input-box");
     inputBox.addEventListener("keydown", commandKeyCb);
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
