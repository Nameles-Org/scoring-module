FROM apastor/debian-zmqpp

ENV RCV_PORT 58501
ENV SND_PORT 58505

WORKDIR /scoring-module

ADD . /scoring-module

RUN apt-get install -y libpqxx-dev libgflags-dev libboost-system-dev libboost-thread-dev

RUN make

EXPOSE $RCV_PORT $SND_PORT

ENTRYPOINT /scoring-module/nameles-scoring -dspIP $DSP_IP -rcvport $RCV_PORT -sndport $SND_PORT \
    -dbIP $DB_IP -dbUSER $DB_USER -dbPWD $DB_PWD -dbNAME $DB_NAME -day $DB_DAY \
    -nWorkers $WORKERS -fwdport $FWD_PORT -min_total $MIN_TOTAL
