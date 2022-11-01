// Create_Node
Node *Create_Node(char *rn, char *ri, char *pi, char *nu, char *sur, char *acpi, char *pv_acor, char *pv_acop, char *pvs_acor, char *pvs_acop) {}

// Parse_URI
Node* Parse_URI(Node *cb, char *uri_array) {
	fprintf(stderr,"Parse_URI \x1b[33m%s\x1b[0m...",uri_array);
	//char uri_array[MAX_URI_SIZE];
	char *uri_parse = uri_array;
	Node *node = NULL;

	//strcpy(uri_array, uri);
	
	uri_parse = strtok(uri_parse, "/");
	
	int viewer = 0, test = 0, la_ol = 0;
	
	if(uri_parse != NULL) {
		if(!strcmp("test", uri_parse)) test = 1;
		else if(!strcmp("viewer", uri_parse)) viewer = 1;
		
		if(test || viewer) uri_parse = strtok(NULL, "/");
	}
	
	if(uri_parse != NULL && !strcmp("TinyIoT", uri_parse)) node = cb;
	
	while(uri_parse != NULL && node) {
		char *cin_ri;
		if((cin_ri = strstr(uri_parse, "4-20")) != NULL) {
			fprintf(stderr,"OK\n\x1b[43mRetrieve CIN By Ri\x1b[0m\n");
			Retrieve_CIN_Ri(cin_ri);
			return NULL;
		}
	
		if(!strcmp("la", uri_parse) || !strcmp("latest", uri_parse)) {
			while(node->siblingRight) {
				if(node->ty == t_CIN && node->siblingRight->ty != t_CIN) break;
				node = node->siblingRight;
			}
			la_ol = 1;
		} else if(!strcmp("ol", uri_parse) || !strcmp("oldest", uri_parse)) {
			while(node) {
				if(node->ty == t_CIN) break;
				node = node->siblingRight;
			}
			la_ol = 1;
		}
		
		if(!la_ol)
		{
			while(node) {
				if(!strcmp(node->rn,uri_parse)) break;
				node = node->siblingRight;
			}
		}

		la_ol = 0;
		
		uri_parse = strtok(NULL, "/");
		
		if(uri_parse == NULL) break;
		
		if(!strcmp(uri_parse, "cinperiod")) {
			fprintf(stderr,"OK\n\x1b[43mRetrieve CIN in Period\x1b[0m\n");
			CIN_in_period(node);
			return NULL;
		}
		
		if(node) node = node->child;
	}
	
	if(node) {
		if(viewer) {
			fprintf(stderr,"OK\n\x1b[43mTree Viewer API\x1b[0m\n");
			Tree_Viewer_API(node);
			return NULL;
		} else if(test) {
			fprintf(stderr,"OK\n\x1b[43mObject Test API\x1b[0m\n");
			Object_Test_API(node);
			return NULL;
		}
	} else if(!node) {
		fprintf(stderr,"Invalid\n");
		HTTP_400;
		printf("{\"m2m:dbg\": \"invalid object\"}");
		return NULL;
	}
	
	fprintf(stderr,"OK\n");

	return node;
}

//Tree_data
void Tree_data(Node *node, char **viewer_data, int cin_num) {
	if(node->ty == t_CIN) {
		Node *cinLatest = Get_CIN_Pi(node->pi);
		
		Node *p = cinLatest;
		
		cinLatest = Latest_CINs(cinLatest, cin_num);
		
		while(cinLatest) {
			char *json = Node_to_json(cinLatest);
			strcat(*viewer_data, ",");
			strcat(*viewer_data, json);
			Node *right = cinLatest->siblingRight;
			Free_Node(cinLatest);
			cinLatest = right;
		}
		return;
	}
	
	char *json = Node_to_json(node);
	strcat(*viewer_data, ",");
	strcat(*viewer_data, json);
	
	node = node->child;
	
	while(node) {
		Tree_data(node, viewer_data, cin_num);
		if(node->ty == t_CIN) {
			while(node->siblingRight && node->siblingRight->ty != t_Sub) {
				node = node->siblingRight;
			}
		}
		node = node->siblingRight;
	}
}
