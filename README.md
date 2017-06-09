# scoring-module
Scoring module for interfacing the DSP. It forwards the requests data to the data processing module

## Running the module as a containerized service with docker-compose

The scoring-module can be easily run with the docker-compose.yml configuration file included in the repository. Adjust the parameters of the configuration file to your scenario.

```bash
sudo docker-compose up
```

The docker image will be pulled automatically from docker hub.

## Building from source

1. Install the dependencies:
  ```bash
  sudo apt-get install make g++ libgflags-dev libboost-thread-dev libpqxx-dev libzmq3-dev
  ```

2. In debian zmqpp is not available in the repositories. Install it from github:
  ```bash
  wget https://github.com/zeromq/zmqpp/archive/4.1.2.tar.gz
  tar zxvf 4.1.2.tar.gz && cd zmqpp-4.1.2
  make && sudo make install && ldconfig
  cd ..
```

3. Build the binaries with make:

  ```bash
  make
  ```

## Running the scoring module

We recommend [running the scoring module as a containerized service with docker-compose](#Running-the-module-as-a-containerized-service-with-docker-compose), however you can run it in native for testing as follows:

```bash
./nameles-scoring -dbIP <127.0.0.1> -dbUSER <user> -dbNAME <nameles>
```

You need to specify at least the database parameters from where to extract the scores. This database is built by the [data-processing-module](https://github.com/Nameles-Org/data-processing-module). While we update the data processing module to be seamlessly integrated with the scoring module, you can build the database from log files. The parameters for the database can be passed from command line.

The program accepts the following command line parameters:

  - -MPS (Queries per second) type: int32 default: 30000
  - -day (Day of the database to use for the hash tables in format YYMMDD))
   type: string default: "161201"
  - -dbIP (IP address of the database, data processing module) type: string
   default: "127.0.0.1"
  - -dbNAME (database name) type: string default: "nameles"
  - -dbPWD (password of the database) type: string default: "password"
  - -dbUSER (database user) type: string default: "user"
  - -dspIP (IP address of the DSP) type: string default: "127.0.0.1"
  - -rcvport ("Receive from" port) type: int32 default: 58501
  - -sndport ("Send to" port) type: int32 default: 58505
  - -fwdport (Data analysis forwarding port) type: int32 default: 58510
  - -min_total (Minimum number of visits to consider a domain score)
      type: int32 default: 250
  - -nWorkers (Number of workers) type: int32 default: 4
