const term =
  document.getElementById("terminal");

const input =
  document.getElementById("command");


function append(text){
  term.textContent += text;
  term.scrollTop = term.scrollHeight;
}


async function run(command){
  command = command.trim();

  if(!command){
    return;
  }

  append(`${command}\n`);

  try{
    const r = await fetch(
      "/api/console/run",
      {
        method:"POST",
        headers:{
          "Content-Type":"application/json"
        },
        body:JSON.stringify({
          command
        })
      }
    );

    const d = await r.json();

    if(d.clear){
      term.textContent =
        "server@vipi:~$ ";

      return;
    }

    if(!r.ok){
      append(
        `ERROR: ${d.detail || "Command failed"}\n`
      );
    }else{
      append(
        `${d.output || ""}\n`
      );
    }

  }catch(e){
    append(`ERROR: ${e.message}\n`);
  }

  append("\nserver@vipi:~$ ");
}


document
  .getElementById("commandForm")
  .addEventListener(
    "submit",
    e => {
      e.preventDefault();

      const cmd = input.value;

      input.value = "";

      run(cmd);
    }
  );
