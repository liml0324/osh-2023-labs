use std::{
    io, 
    io::{prelude::*, BufReader},
    fs,
    net::{TcpListener, TcpStream}
};

fn main() {
    let listener = loop{
        let listener = TcpListener::bind("0.0.0.0:8000");
        match listener {
            Ok(listener) => {
                println!("Established listener successfully!");
                break listener},
            Err(_) => {
                println!("Unable to listen on 0.0.0.0:8000.");
                println!("Enter Y/y to try again, or enter any other keys to exit.");
                let mut choice = String::new();
                io::stdin()
                    .read_line(&mut choice)
                    .expect("Failed to read line.");
                let choice_str = choice.trim();
                if choice_str == "Y" || choice_str == "y"{
                    continue;
                }
                else {
                    return;
                }
            }
        }
    };

    for stream in listener.incoming() {
        let mut stream = stream.unwrap();
        handle_TcpStream(stream);
    }
}

fn handle_TcpStream(mut stream: TcpStream){
    let buf_reader = BufReader::new(& mut stream);
    let mut http_requests:Vec<String> = Vec::new();
    let buf_reader_lines = buf_reader.lines();
    for line in buf_reader_lines {
        let new_line = match line {
            Ok(line) => line,
            Err(_) => {
                let response = "HTTP/1.0 500 Internal Server Error \r\n\r\n";
                stream.write_all(response.as_bytes()).unwrap();
                return;
            },
        };
        if new_line.is_empty() {
            break;
        }
        http_requests.push(new_line);
    }
    println!("Request: {:#?}", http_requests);

    let request_line:Option<&String> = http_requests.get(0);
    let request_line = match request_line {
        Some(line) => line,
        None => {
            let response = "HTTP/1.0 500 Internal Server Error \r\n\r\n";
            stream.write_all(response.as_bytes()).unwrap();
            return;
        },
    };
    let request_line = request_line.split(" ");
    let mut request_line_vec:Vec<&str> = Vec::new();
    for line in request_line {
        let new_line = line.trim();
        if line.is_empty() {
            break;
        }
        request_line_vec.push(line);
    }
    if request_line_vec.len() != 3{
        let response = "HTTP/1.0 500 Internal Server Error \r\n\r\n";
        stream.write_all(response.as_bytes()).unwrap();
        return;
    }
    
    let method = request_line_vec.get(0);
    let method = match method {
        Some(method) => *method,
        None => {
            let response = "HTTP/1.0 500 Internal Server Error \r\n\r\n";
            stream.write_all(response.as_bytes()).unwrap();
            return;
        },
    };

    if method != "GET" {
        let response = "HTTP/1.0 500 Internal Server Error \r\n\r\n";
        stream.write_all(response.as_bytes()).unwrap();
        return;
    }

    let path = request_line_vec.get(1);
    let mut path = match path {
        Some(path) => (*path).to_string(),
        None => {
            let response = "HTTP/1.0 500 Internal Server Error \r\n\r\n";
            stream.write_all(response.as_bytes()).unwrap();
            return;
        },
    };
    path.remove(0);
    println!("{path}");

    let last_dot_pos = path.rfind('.');
    let last_dot_pos = match last_dot_pos {
        Some(pos) => pos,
        None => path.len(),
    };
    println!("{last_dot_pos}");

    if last_dot_pos < path.len()-1 && path[last_dot_pos+1..].trim() == "html" {
        let contents = fs::read_to_string(path);
        match contents {
            Ok(contents) => {
                println!("OK");
                let status_line = String::from("HTTP/1.0 200 OK");
                let length = contents.len();
                let response =
                    format!("{status_line}\r\nContent-Length: {length}\r\n\r\n{contents}");
                stream.write_all(response.as_bytes()).unwrap();
            },
            Err(_) => {
                println!("Err");
                let status_line = String::from("HTTP/1.0 404 NOT FOUND");
                let response =
                    format!("{status_line}\r\n\r\n");
                stream.write_all(response.as_bytes()).unwrap();
            },
        };
        
    }
}