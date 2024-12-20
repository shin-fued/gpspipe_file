#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <cjson/cJSON.h>
#include <unistd.h>
#include <time.h>
#include <unistd.h>
#define GPS_PATH_BASE "/opt/IMAGINE-B5G/realtime/GPS/"
#define SENSOR_GET_CMD "python3 /opt/utils/get_sensor_id.py > sensor.txt"
#define GPSPIPE_CMD "gpspipe -w -n 10 | grep -m 1 lon > temp.txt"
#define COL_NAME "sensor_id,date,lat,long,alt\n"
#define MAX_BUFFER_SIZE 1024




char* reformat_date(char* data, int jt){
    data[11]=' ';
    int i;
    if(jt==1){
        char* time=malloc(8);
        for(i=12; i< 20; i++){
            time[i-12]=data[i];
        }
        return time;
    }
    else{
        char* date=malloc(19);
        for(i=1; i<20; i++){
	    date[i-1]=data[i];
	    //printf("date[i]: %c ", date[i-1]);
        }
	printf("\n");
        return date;
    }
}

char* get_sensor_id() {
    char* line = malloc(5);  // Allocate memory for the sensor ID
    if (line == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        return NULL;
    }
    system(SENSOR_GET_CMD);
    FILE* file = fopen("sensor.txt", "r");
    if (file != NULL) {
        fgets(line, 5, file);
	//printf("line: %s\n", line);
        fclose(file); // Close the file after reading
    } else {
        fprintf(stderr, "Unable to open sensor.txt!\n");
        free(line);
        return NULL;
    }
    remove("sensor.txt");  // Clean up temporary file
    return line;           // Return allocated string
}


void write_row(char* src,char* dest,int start,int stop){
    for (int i=start; i<stop; i++){
        dest[i]=src[i-start];
    }
}

void write_data(const char* data, const char* hist_path, const char* rt_path) {
    if (!data) {
        fprintf(stderr, "No data.\n");
        return;
    }

    char row_rt[1024];
    char row_hist[1024];

    cJSON* data_json = cJSON_Parse(data);
    if (!data_json) {
        fprintf(stderr, "Failed to parse JSON data.\n");
        return;
    }

    FILE* fp_hist = fopen(hist_path, "a");
    FILE* fp_rt = fopen(rt_path, "w");
    if (!fp_hist || !fp_rt) {
        fprintf(stderr, "Failed to open files for writing.\n");
        cJSON_Delete(data_json);
        fclose(fp_hist);
        fclose(fp_rt);
        return;
    }

    // Write headers if history file is empty
    fseek(fp_hist, 0, SEEK_END);
    if (ftell(fp_hist) == 0) {
        fputs(COL_NAME, fp_hist);
    }
    fputs(COL_NAME, fp_rt);


    char* sensor_id = get_sensor_id();
    if (!sensor_id) {
        fclose(fp_hist);
        fclose(fp_rt);
        cJSON_Delete(data_json);
        return;
    }


    int len = strlen(sensor_id);
    write_row(sensor_id, row_hist, 0, len);
    write_row(sensor_id, row_rt, 0, len);
    row_hist[len] = ',';
    row_rt[len] = ',';
    len+=1;
    int len_rt=len;

    char time_stamp[20];
    char tm[9];
    time_t now = time(NULL);
    struct tm* tm_struct = localtime(&now);
    strftime(time_stamp, sizeof(time_stamp), "%Y-%m-%d %H:%M:%S", tm_struct);
    strftime(tm, sizeof(tm), "%H:%M:%S", tm_struct);
    //printf("time_stamp: %s\n", time_stamp);
    //printf("time: %s", tm);
    //free(tm_struct);
    //write into the rows

    write_row(time_stamp, row_hist, len, len+strlen(time_stamp));
    len+=strlen(time_stamp);
    write_row(tm, row_rt, len_rt, len_rt+strlen(tm));
    len_rt+=strlen(tm);
    

    row_hist[len] = ',';
    row_rt[len_rt] = ',';
    len+=1;
    len_rt+=1;

    const cJSON* lat_j = cJSON_GetObjectItemCaseSensitive(data_json, "lat");
    const cJSON* lon_j = cJSON_GetObjectItemCaseSensitive(data_json, "lon");
    const cJSON* alt_j = cJSON_GetObjectItemCaseSensitive(data_json, "alt");

    if (lat_j) {
        char* lat = cJSON_Print(lat_j);
        write_row(lat, row_hist, len, len+strlen(lat));
	write_row(lat, row_rt, len_rt, len_rt+strlen(lat));
	len_rt+=strlen(lat);
	len+=strlen(lat);
	row_hist[len] = ',';
        row_rt[len_rt] = ',';
        len+=1;
        len_rt+=1;
        free(lat);
    }
    if (lon_j) {
        char* lon = cJSON_Print(lon_j);
        write_row(lon, row_hist, len, len+strlen(lon));
	write_row(lon, row_rt, len_rt, len_rt+strlen(lon));
	len_rt+=strlen(lon);
	len+=strlen(lon);
	row_hist[len] = ',';
        row_rt[len_rt] = ',';
        len+=1;
        len_rt+=1;
        free(lon);
    }
    if (alt_j) {
        char* alt = cJSON_Print(alt_j);
        write_row(alt, row_hist, len, len+strlen(alt));
	write_row(alt, row_rt, len_rt, len_rt+strlen(alt));
	len_rt+=strlen(alt);
	len+=strlen(alt);
        free(alt);
    }
    row_hist[len]='\n';
    row_hist[len+1]='\0';
    row_rt[len_rt]='\0';
    //printf("row hist: %s", row_hist);
    //printf("row rt: %s\n", row_rt);

    fputs(row_hist, fp_hist);
    fputs(row_rt, fp_rt);

    free(sensor_id);
    cJSON_Delete(data_json);
    fclose(fp_hist);
    fclose(fp_rt);
}

void get_lat_lon_h(const char* hist_path, const char* rt_path) {
    FILE *fptr;
    system(GPSPIPE_CMD);
    fptr = fopen("temp.txt", "r");
    
    char buffer[2048];
    if (fptr != NULL) {
        while (fgets(buffer, sizeof(buffer), fptr)) {
            write_data(buffer, hist_path, rt_path);
        }
        fclose(fptr);
        remove("temp.txt");  // Remove the temporary file
    } else {
        fprintf(stderr, "Unable to open file!\n");
    }
}

int main() {
    char hist_path[MAX_BUFFER_SIZE], rt_path[MAX_BUFFER_SIZE];
    
    char sensor_id[5];
    char* temp = get_sensor_id();
    if (!temp){
	    perror("Error getting sensor value");
	    return 1;
    }
    strcpy(sensor_id, temp);
    free(temp);
    snprintf(rt_path, sizeof(rt_path), "%s%s_gps_realtime.csv", GPS_PATH_BASE, sensor_id);
    const char* csv = ".csv";
    //printf("rt_path: %s\n", rt_path);
    //printf("rt_path: %s\n", rt_path);
    //printf("hist_path: %s\n", hist_path);
    while (1) {
        char date[11];
        time_t now = time(NULL);
        struct tm* tm_struct = localtime(&now);
        strftime(date, sizeof(date), "%Y-%m-%d", tm_struct);
        //check if our current day path is complete;
        if(strstr(hist_path, csv)==NULL){
            snprintf(hist_path, sizeof(hist_path), "%s%s_gps_%s.csv", GPS_PATH_BASE, sensor_id, date);
        }
        
       	//free(tm_struct);
        get_lat_lon_h(hist_path, rt_path);
        sleep(10);
    }
    return 0;
}

