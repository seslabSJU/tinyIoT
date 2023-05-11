<template>
    <v-container fluid>
        <!-- fluid: 가로로 꽉 채울 때-->
        <v-form>
            <v-container fluid>
                <v-row>
                    <v-col cols="12" md="4">
                        <v-text-field v-model="url" :rules="urlRules" label="Target IP" placeholder="http://localhost:3000" required>
                        </v-text-field>
                    </v-col>
                    <v-col cols="12" md="4">
                        <v-text-field v-model="path" :rules="urlRules" label="Target Resource" placeholder="/TinyIoT" required>
                        </v-text-field>
                    </v-col>
                    <v-col cols="12" md="2">
                        <v-btn class="mr-4" @click="submit" small> <!-- small 변경 -->
                            submit
                        </v-btn>
                        <v-btn @click="clear" small>
                            clear
                        </v-btn>
                    </v-col>
                    <v-col cols="12" md="2">
                        <v-text-field v-model="searchText" dense filled rounded clearable placeholder="Search"
                            prepend-inner-icon="mdi-magnify" :class="{ closed: searchBoxClosed && searchText === '' }"
                            @keyup.enter="discovery(searchText)" @focus="searchBoxClosed = false"
                            @blur="searchBoxClosed = true">
                        </v-text-field>
                    </v-col>
                </v-row>
                <label>Number of Instance</label>
                <v-radio-group v-model="latest" row>
                    <v-radio value="1" label="1 Latest"></v-radio>
                    <v-radio value="3" label="3 Latest"></v-radio>
                    <v-radio value="5" label="5 Latest"></v-radio>
                    <v-radio value="10" label="ALL (up to 10)"></v-radio>
                </v-radio-group>
            </v-container>
            <v-divider></v-divider>
        </v-form>
        <v-container fluid>
            <v-row>
                <v-col cols="12" md="8">
                    <v-treeview :items="treeList" hoverable>
                        <template v-slot:label="{ item }">
                            <span :class="{ blink: newRI === item.ri }" @contextmenu.prevent="show($event, item)"
                            style="display: block;"
                            >
                                <span v-if="item.ty === 5">
                                    <v-chip class="ma-2" color="yellow" label small><strong>CSE</strong></v-chip>
                                </span>
                                <span v-if="item.ty === 2">
                                    <v-chip class="ma-2" color="blue" text-color="white" label small><strong>AE</strong>
                                    </v-chip>
                                </span>
                                <span v-if="item.ty === 3">
                                    <v-chip class="ma-2" color="red" text-color="white" label small><strong>CNT</strong>
                                    </v-chip>
                                </span>
                                <span v-if="item.ty === 4">
                                    <v-chip class="ma-2" color="green" text-color="white" label
                                    small><strong>CIN</strong>
                                </v-chip>
                                </span>
                                <span v-if="item.ty === 9">
                                    <v-chip class="ma-2" color="orange" text-color="white" label
                                    small><strong>GRP</strong>
                                    </v-chip>
                                </span>
                                <span v-if="item.ty === 23">
                                    <v-chip class="ma-2" label small><strong>SUB</strong></v-chip>
                                </span>
                                <span>
                                    {{ item.rn }}
                                </span>
                            </span>
                        </template>
                    </v-treeview>
                    <v-menu v-model="showMenu" :position-x="x" :position-y="y" absolute offset-y close-on-click>
                        <v-list>
                            <v-list-item v-for="(menuItem, index) in menuItems" :key="index"
                                :style="[menuItem.disabled === true ? { 'pointer-events': 'none', 'color': 'silver' } : {}]"
                                @click="menuClick(menuItem)">
                                <v-list-item-title>{{ menuItem.title }}</v-list-item-title>
                            </v-list-item>
                            <!-- enabled만 보이기 -->
                        </v-list>
                    </v-menu>
                </v-col>
                <v-col cols="12" md="4">
                    <template v-if="clickMenu === true">
                        <v-card class="my-auto">
                            <v-app-bar dark color="#17a2b7">
                                <v-toolbar-title>
                                    <span v-if="selectMenu === 'c'">Create a child resource</span>
                                    <span v-if="selectMenu === 'd'">Delete the selected resource</span>
                                    <span v-if="selectMenu === 'p'">Resource Information</span>
                                </v-toolbar-title>
                                <v-spacer></v-spacer>
                                <v-btn icon>
                                    <v-icon @click="close">mdi-close</v-icon>
                                </v-btn>
                            </v-app-bar>
                            <div v-if="selectMenu === 'c'">
                                <v-card-text>1. Select resource type</v-card-text>
                                <div style="padding-left: 15px">
                                    <label>
                                        <input type="radio" id="cnt" name="selectResource" value="cnt" v-model="selectR"
                                            @click="clickRadio()">
                                        <v-chip class="ma-2" color="red" text-color="white" label small>
                                            <label for="cnt"><strong>CNT</strong></label>
                                        </v-chip>
                                    </label>
                                    &nbsp;&nbsp;
                                    <label>
                                        <input type="radio" id="cin" name="selectResource" value="cin" v-model="selectR"
                                            @click="clickRadio()" :disabled="disableR === true">
                                        <v-chip class="ma-2" color="green" text-color="white" label small>
                                            <label for="cin"><strong>CIN</strong></label>
                                        </v-chip>
                                    </label>
                                    &nbsp;&nbsp;
                                    <label>
                                        <input type="radio" id="sub" name="selectResource" value="sub" v-model="selectR"
                                            @click="clickRadio()" :disabled="disableR === true">
                                        <v-chip class="ma-2" label small>
                                            <label for="sub"><strong>SUB</strong></label>
                                        </v-chip>
                                    </label>
                                </div>
                                <v-divider></v-divider>
                                <v-card-text>2. Fill out resource information</v-card-text>
                                <v-container fluid>
                                    <v-row v-if="selectR === 'cnt' || selectR === 'sub'">
                                        <v-col cols="12" md="4">
                                            <v-card-text>Resource Name (rn)</v-card-text>
                                        </v-col>
                                        <v-col cols="12" md="8">
                                            <v-text-field v-model="createRn" label="Resource Name" required>
                                            </v-text-field>
                                        </v-col>
                                    </v-row>
                                    <v-row v-if="selectR === 'cin'">
                                        <v-col cols="12" md="4">
                                            <v-card-text>Content (con)</v-card-text>
                                        </v-col>
                                        <v-col cols="12" md="8">
                                            <v-text-field v-model="createCon" :rules="conRules" label="Content" required>
                                            </v-text-field>
                                        </v-col>
                                    </v-row>
                                    <v-row v-if="selectR === 'sub'">
                                        <v-col cols="12" md="4">
                                            <v-card-text>Event Noti. Uri (nu)</v-card-text>
                                        </v-col>
                                        <v-col cols="12" md="8">
                                            <v-text-field v-model="createNu" :rules="nuRules" label="Event Noti. Uri" required>
                                            </v-text-field>
                                        </v-col>
                                    </v-row>
                                    <v-row v-if="selectR === 'sub'">
                                        <v-col cols="12" md="4">
                                            <v-card-text>Event Noti. Criteria (enc)</v-card-text>
                                        </v-col>
                                        <v-col cols="12" md="8">
                                            <v-checkbox v-model="createNet" label="Update of Resource(1)" :value=1>
                                            </v-checkbox>
                                            <v-checkbox v-model="createNet" label="Delete of Resource(2)" :value=2>
                                            </v-checkbox>
                                            <v-checkbox v-model="createNet" label="Create of Direct Child Resource(3)"
                                                :value=3></v-checkbox>
                                            <v-checkbox v-model="createNet" label="Delete of Direct Child Resource(4)"
                                                :value=4></v-checkbox>
                                        </v-col>
                                    </v-row>
                                </v-container>
                            </div>
                            <div v-if="selectMenu === 'd'">
                                <v-card-text>
                                    <p>When you click "Delete" button, the selected resource will be removed from server
                                        include it's descendant resources and this process is not recoverable. Confirm
                                        what you want to do!</p>
                                    <p> Are you sure you want to delete
                                        <span v-if="selectTy === 5">
                                            <v-chip class="ma-2" color="yellow" label small><strong>CSE</strong>
                                            </v-chip>
                                        </span>
                                        <span v-if="selectTy === 2">
                                            <v-chip class="ma-2" color="blue" text-color="white" label small>
                                                <strong>AE</strong>
                                            </v-chip>
                                        </span>
                                        <span v-if="selectTy === 3">
                                            <v-chip class="ma-2" color="red" text-color="white" label small>
                                                <strong>CNT</strong>
                                            </v-chip>
                                        </span>
                                        <span v-if="selectTy === 4">
                                            <v-chip class="ma-2" color="green" text-color="white" label small>
                                                <strong>CIN</strong>
                                            </v-chip>
                                        </span>
                                        <span v-if="selectTy === 9">
                                            <v-chip class="ma-2" color="orange" text-color="white" label
                                                small><strong>GRP</strong>
                                            </v-chip>
                                        </span>
                                        <span v-if="selectTy === 23">
                                            <v-chip class="ma-2" label small><strong>SUB</strong></v-chip>
                                        </span>
                                        <span><strong>{{ selectRn }} </strong>?</span>
                                    </p>
                                </v-card-text>
                            </div>
                            <div v-if="selectMenu === 'p'">
                                <v-card-title>
                                    <span v-if="selectTy === 5">
                                        <v-chip class="ma-2" color="yellow" label small><strong>CSE</strong>
                                        </v-chip>
                                    </span>
                                    <span v-if="selectTy === 2">
                                        <v-chip class="ma-2" color="blue" text-color="white" label small>
                                            <strong>AE</strong>
                                        </v-chip>
                                    </span>
                                    <span v-if="selectTy === 3">
                                        <v-chip class="ma-2" color="red" text-color="white" label small>
                                            <strong>CNT</strong>
                                        </v-chip>
                                    </span>
                                    <span v-if="selectTy === 4">
                                        <v-chip class="ma-2" color="green" text-color="white" label small>
                                            <strong>CIN</strong>
                                        </v-chip>
                                    </span>
                                    <span v-if="selectTy === 9">
                                        <v-chip class="ma-2" color="orange" text-color="white" label
                                            small><strong>GRP</strong>
                                        </v-chip>
                                    </span>
                                    <span v-if="selectTy === 23">
                                        <v-chip class="ma-2" label small><strong>SUB</strong></v-chip>
                                    </span>
                                    <span><strong>{{ selectRn }} </strong></span>
                                </v-card-title>
                                <v-simple-table v-for="item in resource">
                                    <template v-slot:default>
                                        <thead>
                                            <tr bgcolor="#E5EEFA">
                                                <th class="text-left">Attribute</th>
                                                <th class="text-left">Value</th>
                                            </tr>
                                        </thead>
                                        <tbody>
                                            <tr v-for="(value, name) in item">
                                                <td>{{ name }}</td>
                                                <td>{{ value }}</td>
                                            </tr>
                                        </tbody>
                                    </template>
                                </v-simple-table>
                            </div>

                            <v-card-actions>
                                <v-spacer></v-spacer>
                                <v-btn v-if="selectMenu === 'c'" class="ma-1" color="error" plain @click="create">
                                    Create
                                </v-btn>
                                <v-btn v-if="selectMenu === 'd'" class="ma-1" color="error" plain @click="remove">
                                    Delete
                                </v-btn>
                            </v-card-actions>
                        </v-card>
                    </template>
                </v-col>
            </v-row>
        </v-container>
    </v-container>
</template>

<script>
import axios from 'axios'

export default {
    data: () => ({
        url: '',
        path: '',
        urlRules: [
            v => !!v || 'URL is required'
        ],
        searchText: '', // Discovery
        searchBoxClosed: true,
        latest: '10',

        // Tree
        list: [],
        treeList: [],

        // ContextMenu
        showMenu: false,
        x: 0,
        y: 0,
        menuItems: [
            { title: 'Create', disabled: false },
            { title: 'Delete', disabled: false },
            { title: 'Properties', disabled: false },
        ],

        // Menu
        clickMenu: false,
        selectMenu: '',
        selectRn: '',
        selectTy: '',
        selectR: 'cnt',
        disableR: false,
        createRn: '',
        createCon: '',
        conRules: [
            v => !!v || 'Content is required'
        ],
        createNu: [],
        nuRules: [
            v => v === 0 || 'Noti URI is required'
        ],
        createNet: [],
        resource: {},
        newRI: '',

    }),
    methods: {
        list_to_tree(list) {
            var map = {}, node, roots = [], i;
            for (i = 0; i < list.length; i += 1) {
                map[list[i].ri] = i; // initialize the map
                list[i].children = []; // initialize the children
            }
            for (i = 0; i < list.length; i += 1) {
                node = list[i];
                if (node.pi !== "NULL") {
                    // if you have dangling branches check that map[node.parentId] exists
                    list[map[node.pi]].children.push(node);
                }
                else {
                    roots.push(node);
                }
            }
            return roots;
        },

        async submit() {
            var url = this.url + '/viewer' + this.path + '?fu=1&la=' + this.latest;

            await axios.get(url)
                .then(response => {
                    this.list = response.data;
                    console.log(this.list);
                })
                .catch(error => {
                    console.log(error);
                    this.$swal.fire({
                        title: '대상 서버에 접속할 수 없습니다',
                        text: 'Unable to access destination server',
                        icon: 'error'
                    })
                });

            this.treeList = this.list_to_tree(this.list);
            console.log(this.treeList);
        },

        clear() {
            Object.assign(this.$data, this.$options.data.call(this));
        },

        async discovery(searchText) { // 미구현
            console.log(this.url + this.path + '?fur');
        },

        // ContextMenu
        show(e, item) {
            e.preventDefault();
            this.showMenu = false;
            this.x = e.clientX;
            this.y = e.clientY;
            this.$nextTick(() => {
                this.showMenu = true;
            });

            if(this.selectRn === item.rn){
                return;
            }else{
                this.resource = {};
            }

            if (item.ty === 5) {
                console.log('CSE');
                this.menuItems[0].disabled = true;
                this.menuItems[1].disabled = true;
            }
            else if (item.ty === 2) {
                console.log('AE');
                this.menuItems[0].disabled = false;
                this.menuItems[1].disabled = true;
            }
            else if (item.ty === 3) {
                console.log('CNT');
                this.menuItems[0].disabled = false;
                this.menuItems[1].disabled = false;
            }
            else if (item.ty === 4) {
                console.log('CIN');
                this.menuItems[0].disabled = true;
                this.menuItems[1].disabled = true;
            }
            else if (item.ty === 9){
                console.log('GRP');
                this.menuItems[0].disabled = false;
                this.menuItems[0].disabled = false;
            }
            else if (item.ty === 23) {
                console.log('SUB');
                this.menuItems[0].disabled = true;
                this.menuItems[1].disabled = false;
            }

            this.selectRn = item.rn;
            this.selectTy = item.ty;
        },

        menuClick(menuItem) {
            this.clickMenu = true;
            console.log('selectRn: ' + this.selectRn);
            console.log('selectTy: ' + this.selectTy);

            if (menuItem.title === 'Create') {
                console.log('Create');
                this.selectMenu = 'c';
                if (this.selectTy === 2)
                    this.disableR = true;
            }
            else if (menuItem.title === 'Delete') {
                console.log('Delete');
                this.selectMenu = 'd';
            }
            else if (menuItem.title === 'Properties') {
                console.log('Properties');
                this.selectMenu = 'p';
                this.properties();
            }

        },

        close() {
            this.clickMenu = false;
            this.selectMenu = '';
            this.selectRn = '';
            this.selectTy = '';
            this.selectR = 'cnt';
            this.disableR = false;
            this.createRn = '';
            this.createCon = '';
            this.createNu = [];
            this.createNet = [];
            this.resource = {};
        },

        clickRadio() {
            this.selectR = document.querySelector('input[name="selectResource"]:checked').value;
            this.createRn = '';
            this.createCon = '';
            this.createNu = [];
            this.createNet = [];
            this.resource = {};
        },

        async create() {
            var path = this.findPath(this.list, this.selectRn);
            var url = this.url + path;

            if (this.selectR === 'cnt') {
                var data = {
                    "m2m:cnt": {
                        "rn": this.createRn
                    }
                }

                const jsonData = JSON.stringify(data);
                console.log(jsonData);

                await axios.post(url, jsonData, { headers: { 'Content-Type': 'application/json; ty=3', "X-M2M-Origin":"COrigin",
                    "X-M2M-RI":"12345" } })
                    .then((response) => {
                        this.newRI = response.data["m2m:cnt"].ri;
                    })
                    .catch((error) => {
                        console.log(error);
                    })
            }
            else if (this.selectR === 'cin') {
                var data = {
                    "m2m:cin": {
                        "con": this.createCon
                    }
                }

                const jsonData = JSON.stringify(data);
                console.log(jsonData);

                await axios.post(url, jsonData, { headers: { 'Content-Type': 'application/json; ty=4', "X-M2M-Origin":"COrigin",
                    "X-M2M-RI":"12345" } })
                    .then((response) => {
                        this.newRI = response.data["m2m:cin"].ri;
                    })
                    .catch((error) => {
                        console.log(error);
                    });
            }
            else if (this.selectR === 'sub') {
                if (this.createNu.includes(' '))
                    this.createNu = this.createNu.replace(' ', '') // 공백 제거
                this.createNu = this.createNu.split(',');

                var data = {
                    "m2m:sub": {
                        "rn": this.createRn,
                        "enc": {
                            "net": this.createNet
                        },
                        "nu": this.createNu
                    }
                }

                const jsonData = JSON.stringify(data);
                console.log(jsonData);

                await axios.post(url, jsonData, { headers: { 'Content-Type': 'application/json; ty=23' } })
                    .then((response) => {
                        this.newRI = response.data["m2m:sub"].ri;
                    })
                    .catch((error) => {
                        console.log(error);
                    });
            }

            console.log('생성완료!');
            // 트리 재조회
            this.submit();
            this.close();
        },

        async remove() {
            var path = this.findPath(this.list, this.selectRn);
            var url = this.url + path;

            await axios.delete(url, { headers: { "X-M2M-Origin":"COrigin",
                    "X-M2M-RI":"12345" } } )
                .then((response) => {
                    console.log(response.data);
                    this.$swal.fire({
                        title: 'Deleted!',
                        text: 'The resource has been deleted',
                        icon: 'success'
                    })
                })
                .catch((error) => {
                    console.log(error);
                    this.$swal.fire({
                        title: 'Error!',
                        text: 'The resource has not deleted',
                        icon: 'error'
                    })
                });

            // 트리 재조회
            this.submit();
            this.close();
        },

        findPath(list, selectRn) {
            var result = list.find(item => item.rn === selectRn);
            var pi = result.pi;
            var path = '/' + result.rn;
            while (pi !== "NULL") {
                result = list.find(item => item.ri === pi);
                pi = result.pi;
                path = '/' + result.rn + path;
            }
            return path;
        },

        async properties() {

            var path = this.findPath(this.list, this.selectRn);
            var url = this.url + path;
            console.log(path, url);
            await axios.get(url, {headers:{
                    "X-M2M-Origin":"COrigin",
                    "X-M2M-RI":"12345"
                }})
                .then(response => {
                    console.log(response);
                    this.resource = response.data;
                })
                .catch(error => {
                    console.log(error);
                });

            console.log(this.resource);
        },
    }
}
</script>

<style>
@keyframes blink-effect {
    50% {
        opacity: 0;
    }
}

.blink {
    animation-name: blink-effect;
    animation-duration: 1s;
    animation-iteration-count: 10;  
}
</style>
