typedef struct {
	char *s;
} string_t;


int InStr(int start, char *src, char *word) {
	int srcl = strlen(src);
	int wordl = strlen(word);
	int endi = srcl - wordl+1;
	int i,j;
	if (endi<start) { return -1;}
	for (i=start; i<endi; i++)
	{
		for (j=0;j<wordl;j++)
		{
			if (src[i+j] != word[j])
			{
				goto break1;
			}		
		}
		return i;
		break1:;
	}
	return -1;
}

int strsplit(string_t *sa, char *str, char *delm){
	int sl = strlen(str);
	if (sl == 0) return 0;
	int dl = strlen(delm);
	
	int i,p,l;
	int idx = 0;

	
	i=0;p=0;
	while (1){
		i = InStr( i, str, delm);

		if (i != -1){
			l = i - p;
			//if (strlen(sa[idx].s)>0) free(&sa[idx].s[0]);
			
			sa[idx].s = malloc(l+1);
			memcpy (sa[idx].s,&str[p],l);
			sa[idx].s[l]=0;
			idx += 1;
			p = i + dl;
			i = p;
			
			if (i >= sl) {break;}
		}else{
			//if (p != 0) {
				l = sl - p;
				//if (strlen(sa[idx].s)>0) free(&sa[idx].s[0]);
				if (l>0) {
					sa[idx].s = malloc(l+1);
					memcpy (sa[idx].s,&str[p],l);
					sa[idx].s[l]=0;
					idx += 1;
				}
				break;
			//}else{
			//	break;
			//}
		}
	}
	
	return idx;
}

char *getDir(char *path) {
	int sl= strlen(path);
	if (sl<=0) { return "";}
	char *dr;
	int i,k;
	
	for (i=0;i<sl;i++){
		if (path[i]=='/'){
			k=i;
		}		
	}
	if (k==0) { return "";}


	dr = malloc(k+1);
	memcpy (dr,path,k+1);
	dr[k]=0;
	
	return dr;
	
}

int makeDir(char *path) {
	int i,l;
	char tmps[256];
	char c;
	l=strlen(path);
	if (l<=1)
	{
		return 1;
	}
	for (i=1;i<l;i++)
	{
		c = path[i];
		if (c == '/')
		{
			memcpy(tmps,path,i);
			tmps[i] = 0;
			mkdir(tmps,S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
		}
		if (i==l-1)
		{
			mkdir(path,S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
		}
	}
}
