//gcc gps.c -o gps -lcjson && ./gps && rm gps

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <cjson/cJSON.h>
#include <unistd.h>
#include <time.h>
#include <unistd.h>

static const char* col_name="sensor_id,date,lat,long,alt\n";
static char gps_path[100] = "/opt/IMAGINE-B5G/realtime/GPS/";
static char gps_path_rt[100] = "/opt/IMAGINE-B5G/realtime/GPS/";


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
    system("python3 /opt/utils/get_sensor_id.py > sensor.txt");
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

char* strip_path(char* name){
	int n;
	while(name[n]<'\0'){n++;}
	char* path = malloc(n);
    for(int i=0; i<n; i++){
        path[i]=name[i];
    }
	return path;
}

void write_data(char* data) {
    char row_rt[1024];
    char row_hist[1024];
    const cJSON *lat_j = NULL;
    const cJSON *lon_j = NULL;
    const cJSON *alt_j = NULL;
    const cJSON *date_j = NULL;
    cJSON *data_json = cJSON_Parse(data);
    if (data_json == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            fprintf(stderr, "Error before: %s\n", error_ptr);
        }
        return;  // Exit if parsing failed
    }
    char* path = strip_path(gps_path);
    char* path_rt = strip_path(gps_path_rt);
    FILE* fp;
    if (access(path, F_OK) == 0) {
	    fp = fopen(path, "a");
    } 
    else {
	    fp =fopen(path, "a");
	    fputs(col_name,fp);
    }
    FILE* fp_rt = fopen(path_rt, "w");
    
    lat_j = cJSON_GetObjectItemCaseSensitive(data_json, "lat");
    lon_j = cJSON_GetObjectItemCaseSensitive(data_json, "lon");
    alt_j = cJSON_GetObjectItemCaseSensitive(data_json, "alt");
    date_j = cJSON_GetObjectItemCaseSensitive(data_json, "time");
    char* sensor_id =get_sensor_id();
    // printf("sensor_id: %s\n", sensor_id);
    int len = strlen(sensor_id);
    write_row(sensor_id, row_hist, 0, len);
    write_row(sensor_id, row_rt, 0, len);
    row_hist[len] = ',';
    row_rt[len] = ',';
    len+=1;
    int len_rt=len;

    if (date_j) {
        char *unf_date = cJSON_Print(date_j);
        char* date = reformat_date(unf_date, 0);
        char* time = reformat_date(unf_date, 1);
        write_row(date, row_hist, len, len+strlen(date));
        len+=strlen(date);
        write_row(time, row_rt, len_rt, len_rt+strlen(time));
	    len_rt+=strlen(time);
        //printf("time: %s\n", time);
	//printf("date: %s\n", date);
	    row_hist[len] = ',';
	    row_rt[len_rt] = ',';
	    len+=1;
	    len_rt+=1;
        free(time);
        free(date);
    }
    
    if (lat_j) {
        char *lat = cJSON_Print(lat_j);
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
        char *lon = cJSON_Print(lon_j);
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
        char *alt = cJSON_Print(alt_j);
        write_row(alt, row_hist, len, len+strlen(alt));
        write_row(alt, row_rt, len_rt, len_rt+strlen(alt));
        len_rt+=strlen(alt);
        len+=strlen(alt);
        row_hist[len] = ',';
        row_rt[len_rt] = ',';
        len+=1;
        len_rt+=1;
        free(alt);
    }
    row_hist[len]='\0';
    row_rt[len_rt]='\0';
    printf("row_rt: %s\n", row_rt);
    printf("row_hist: %s\n", row_hist);
    fputs(row_hist, fp);
    char rt_temp[1024];
    strcpy(rt_temp, col_name);
    strcat(rt_temp, row_rt);
    char* rt_data=strip_path(rt_temp);
    fputs(rt_data, fp_rt);
    free(sensor_id);
    cJSON_Delete(data_json);  // Free the cJSON object
    
    fclose(fp);
    fclose(fp_rt);
    free(path);
    free(path_rt);
    free(rt_data);
}

void get_lat_lon_h() {
    FILE *fptr;
    char* cmd = "gpspipe -w -n 10 | grep -m 1 lon > temp.txt";
    system(cmd);
    fptr = fopen("temp.txt", "r");
    
    char buffer[2048];
    if (fptr != NULL) {
        while (fgets(buffer, sizeof(buffer), fptr)) {
            write_data(buffer);
        }
        fclose(fptr);
        remove("temp.txt");  // Remove the temporary file
    } else {
        fprintf(stderr, "Unable to open file!\n");
    }
}

int main() {
    while (1) {
        char dt[11];  // Buffer to hold the date in "YYYY-MM-DD" format
        time_t now = time(NULL);  // Get the current time
        struct tm *tm_struct = localtime(&now);  // Convert to local time structure

        if (tm_struct != NULL) {
            // Format the date into the string
            strftime(dt, sizeof(dt), "%Y-%m-%d", tm_struct);
        } else {
            fprintf(stderr, "Failed to retrieve the current time.\n");
        }
        char* sensor=get_sensor_id();
	    if (!sensor) {
            break;
        }
	    char sensor_id[5];
        for(int i=0; i<5; i++){
            sensor_id[i]=sensor[i];
        }
        free(sensor);
	//printf("sensor: %s\n", sensor_id);
        strcat(gps_path, sensor_id);
        strcat(gps_path_rt, sensor_id);

        strcat(gps_path, "_gps_");
        strcat(gps_path_rt, "_gps_realtime.csv\0");
	printf("date: %s", dt);
        strcat(gps_path, dt);
        strcat(gps_path, ".csv\0");
        printf("gps_path: %s", gps_path);
	get_lat_lon_h();
        sleep(10);  // Sleep for 10 seconds
    }
    return 0;
}
