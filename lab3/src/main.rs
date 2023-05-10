use std::{
    io::{prelude::*, BufReader},
    fs,
    net::{TcpListener, TcpStream},
    thread::{self, JoinHandle},
    sync::{mpsc, Arc, Mutex},
    env,//fmt::{format, Debug},
};

struct ThreadPool {
    threads: Vec<Option<(usize, JoinHandle<()>)>>,
    sender: Option<mpsc::Sender<Box<dyn FnOnce() + Send + 'static>>>,
    debug: bool,
}

impl ThreadPool {
    fn new(size: usize, debug: bool) -> ThreadPool {
        assert!(size > 0);//the other part of the program will grantee that size is greater than 0

        let (sender, receiver) 
            = mpsc::channel::<Box<dyn FnOnce() + Send + 'static>>();

        let receiver = Arc::new(Mutex::new(receiver));

        let sender = Some(sender);

        let mut threads = Vec::with_capacity(size);

        for id in 0..size {
            let receiver = Arc::clone(&receiver);
            let thread = thread::spawn(move || loop {
                let id = id;
                let job = receiver
                    .lock().expect("Thread failed to get lock.")
                    .recv();
                match job {
                    Ok(job) => {
                        if debug {
                            println!("Thread {} begin to handle request.", id);
                        }
                        job();
                    }
                    Err(_) => {
                        if debug {
                            println!("Thread {} disconnected. Begin to shut down.", id);
                        }
                    }
                }
            });
            threads.push(Some((id, thread)));
        }

        ThreadPool { threads, sender, debug }
    }

    fn execute<F>(&self, f: F)
    where
        F: FnOnce() + Send + 'static,
    {
        let job = Box::new(f);
        match self.sender.as_ref() {
            Some(sender) => {
                match sender.send(job){
                    Ok(_) => (),
                    Err(_) => if self.debug {
                        println!("Failed to send job.");
                    }
                }
            }
            None => {
                if self.debug {
                    println!("Could not find sender.");
                }
            }
        }
    }
}

impl Drop for ThreadPool {
    fn drop(&mut self) {
        drop(self.sender.take());
        for thread in &mut self.threads {
            if let Some(thread) = thread.take(){
                if self.debug {
                    println!("Shutting down worker {}", thread.0);
                }
                let result = thread.1.join();
                match result {
                    Ok(_) => (),
                    Err(_) => {
                        if self.debug {
                            println!("Failed to join thread {}", thread.0);
                        }
                    }
                }
            }
        }
    }
}

fn main() {
    const MAX_THREAD_NUM:usize = 20;
    let args:Vec<String> = env::args().collect();
    let mut debug = false;
    let mut thread_num = 4;
    let mut port = "8000";

    for i in 1..args.len() {
        if args[i] == "-d" {
            println!("Debug mode is on.");
            debug = true;
        }
        else if args[i] == "-t" {
            if i+1 < args.len() {
                thread_num = match args[i+1].parse::<usize>() {
                    Ok(t) => {
                        if t <= 0 {
                            println!("Invalid thread number, use 4 as default thread number.");
                            4
                        }
                        else if t > MAX_THREAD_NUM {
                            println!("Thread num is too big. Use 20 as max thread number.");
                            20
                        }
                        else {
                            t
                        }
                    }
                    Err(_) => {
                        println!("Invalid thread number, use 4 as default thread number.");
                        4
                    }
                };
            }
            else {
                println!("Thread number can't be empty, use 4 as default thread number.");
            }
        }
        else if args[i] == "-p" {
            if i+1 < args.len() {
                port = match args[i+1].parse::<usize>() {
                    Ok(_) => args[i+1].as_str(),
                    Err(_) => {
                        println!("Invalid port number, use 8000 as default port.");
                        "8000"
                    }
                };
            }
            else {
                println!("Port number can't be empty, use 8000 as default port.");
            }
        }
    }

    let addr = format!("0.0.0.0:{}", port);
    let listener = TcpListener::bind(addr);
    let listener = match listener {
        Ok(listener) => {
            println!("Established listener successfully on 0.0.0.0:{}.", port);
            println!("Using {} threads.", thread_num);
            listener},
        Err(_) => {
            println!("Unable to listen on 0.0.0.0:{}.", port);
            return;
        }
    };

    let thread_pool = ThreadPool::new(thread_num, debug);

    for stream in listener.incoming() {
        let stream = stream.unwrap();
        thread_pool.execute(move || {
            handle_tcp_stream(stream, debug);
        });
    }
}

fn handle_tcp_stream(mut stream: TcpStream, debug: bool){
    let buf_reader = BufReader::new(& mut stream);
    let mut http_request:Vec<String> = Vec::new();
    let buf_reader_lines = buf_reader.lines();//read request
    for line in buf_reader_lines {
        let new_line = match line {
            Ok(line) => line,
            Err(_) => {
                handle_500(stream, debug);
                return;
            },
        };
        if new_line.is_empty() {
            break;
        }
        http_request.push(new_line);//save request in vector
    }
    if debug {
        println!("Request: {:#?}", http_request);
    }

    let request_line:Option<&String> = http_request.get(0);
    let request_line = match request_line {
        Some(line) => line,
        None => {
            handle_500(stream, debug);
            return;
        },
    };
    // let request_line = request_line.split(" ");
    // let mut request_line_vec:Vec<&str> = Vec::new();
    // for line in request_line {
    //     if line.is_empty() {
    //         break;
    //     }
    //     let new_line = line.trim();
    //     request_line_vec.push(new_line);
    // }
    // if request_line_vec.len() != 3{
    //     handle_500(stream, &debug);
    //     return;
    // }
    let request_line = request_line.trim();
    let first_space_pos = request_line.find(' ');
    let last_space_pos = request_line.rfind(' ');
    let first_space_pos = match first_space_pos {
        Some(pos) => pos,
        None => request_line.len(),
    };
    let last_space_pos = match last_space_pos {
        Some(pos) => pos,
        None => request_line.len(),
    };

    if first_space_pos == request_line.len() || last_space_pos == request_line.len() {//incomplete request line
        handle_500(stream, debug);
        return;
    }
    
    let method = request_line[..first_space_pos].trim();

    if method != "GET" {
        handle_500(stream, debug);
        return;
    }

    let path = request_line[first_space_pos..last_space_pos].trim();
    if path.len() == 0 {
        handle_500(stream, debug);
        return;
    }

    let mut path = path.to_string();

    if path[0..1].to_string() == "/" {
        path.remove(0);
    }

    if debug { 
        println!("Path: {}", path); 
    }

    let last_dot_pos = path.rfind('.');
    let last_dot_pos = match last_dot_pos {
        Some(pos) => pos,
        None => path.len(),
    };

    let mut file_type = "";

    if path.len() > 0 && last_dot_pos < path.len()-1 {
        file_type = path[last_dot_pos+1..].trim();
    }

    if path.len() == 0 {
        handle_404(stream, debug);
    }
    else if last_dot_pos < path.len()-1 && file_type == "html" || file_type == "txt"
        || file_type == "htm" || file_type == "css" || file_type == "js" {
        let contents = fs::read_to_string(path);
        match contents {
            Ok(contents) => {
                handle_200_string(stream, contents, debug);
            },
            Err(error) => {//file not found
                if error.kind() == std::io::ErrorKind::NotFound {
                    handle_404(stream, debug);
                    return;
                }
                else {
                    handle_500(stream, debug);
                    return;
                }
            },
        };
    }
    else {
        let contents = fs::read(path);
        match contents {
            Ok(contents) => {
                handle_200_u8(stream, contents, debug);
            },
            Err(error) => {//file not found
                if error.kind() == std::io::ErrorKind::NotFound {
                    handle_404(stream, debug);
                    return;
                }
                else {
                    handle_500(stream, debug);
                    return;
                }
            },
        };
    }
}

fn handle_404(mut stream: TcpStream, debug: bool) {
    let content = "404 NOT FOUND";
    let response = format!("HTTP/1.0 404 NOT FOUND \r\nContent-Length: {}\r\n\r\n{}",
        content.len(), content);
    let result = stream.write_all(response.as_bytes());
    match result {
        Ok(_) => (),
        Err(_) => {
            if debug { 
                println!("failed to write to stream.");
            }
        }
    }
    if debug {
        println!("Respond: [\r\n{response}\r\n]");
    }
}

fn handle_500(mut stream: TcpStream, debug: bool) {
    let content = "500 Internal Server Error";
    let response = format!("HTTP/1.0 500 Internal Server Error \r\nContent-Length: {}\r\n\r\n{}",
        content.len(), content);
    let result = stream.write_all(response.as_bytes());
    match result {
        Ok(_) => (),
        Err(_) => {
            if debug { 
                println!("failed to write to stream.");
            }
        }
    }
    if debug {
        println!("Respond: [\r\n{response}\r\n]");
    }
}

fn handle_200_u8(mut stream: TcpStream, contents: Vec<u8>, debug: bool) {
    let status_line = String::from("HTTP/1.0 200 OK");
    let length = contents.len();
    let response =
    format!("{status_line}\r\nContent-Length: {length}\r\n\r\n");
    let result = stream.write_all(response.as_bytes());
    match result {
        Ok(_) => (),
        Err(_) => {
            if debug { 
                println!("failed to write to stream.");
            }
        }
    }
    let result = stream.write_all(&contents);
    match result {
        Ok(_) => (),
        Err(_) => {
            if debug { 
                println!("failed to write to stream.");
            }
        }
    }
    if debug {
        println!("Respond: [\r\n{response}\r\n]");
    }
}

fn handle_200_string(mut stream: TcpStream, contents: String, debug: bool) {
    let status_line = String::from("HTTP/1.0 200 OK");
    let length = contents.len();
    let response =
        format!("{status_line}\r\nContent-Length: {length}\r\n\r\n{contents}");
    let result = stream.write_all(response.as_bytes());
    match result {
        Ok(_) => (),
        Err(_) => {
            if debug { 
                println!("failed to write to stream.");
            }
        }
    }
    if debug {
        println!("Respond: [\r\n{response}\r\n]");
    }
}