FROM ubuntu:16.04

RUN apt-get update -y \
  && apt-get install -y build-essential libmysqlclient-dev libreadline-dev mysql-client cmake \
  && rm -rf /var/lib/apt/lists/*

ADD CMakeLists.txt /app/
ADD source /app/source

RUN cd /app \
  && cmake . \
  && make -j4 \
  && mkdir /release \
  && mv world /release \
  && rm -rf /app

RUN mkdir /scripts \
  && ln -s /data/Spells /release/Spells \
  && ln -s /data/Quests /release/Quests \
  && ln -s /data/ItemScripts /release/ItemScripts \
  && ln -s /data/ZoneScripts /release/ZoneScripts \
  && ln -s /data/SpawnScripts /release/SpawnScripts \
  && ln -s /data/log_config.xml /release/log_config.xml \
  && ln -s /data/CommonStructs.xml /release/CommonStructs.xml \
  && ln -s /data/EQ2_Structs.xml /release/EQ2_Structs.xml \
  && ln -s /data/ItemStructs.xml /release/ItemStructs.xml \
  && ln -s /data/SpawnStructs.xml /release/SpawnStructs.xml \
  && ln -s /data/WorldStructs.xml /release/WorldStructs.xml

WORKDIR /release

ENTRYPOINT ["./world"]
